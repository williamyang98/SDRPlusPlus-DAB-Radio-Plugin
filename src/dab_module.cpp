#include "./dab_module.h"
#include <complex>
#include <string>
#include <thread>
#include <chrono>
#include <stdint.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <module.h>
#include <dsp/sink.h>
#include <signal_path/signal_path.h>
#include "./radio_block.h"
#include "./render_radio_block.h"
#include "utility/span.h"

ConfigManager config; // extern

int OFDM_Demodulator_Sink::run() {
    int count = base_type::_in->read();
    if (count < 0) return -1;
    auto* buf = base_type::_in->readBuf;
    auto block = tcb::span(reinterpret_cast<std::complex<float>*>(buf), size_t(count));
    m_ofdm_demod->Process(block);
    base_type::_in->flush();
    return count;
}

Audio_Player_Stream::Audio_Player_Stream(float sample_rate, float block_size_seconds)
: m_sample_rate(sample_rate), m_block_size_seconds(block_size_seconds)
{
    m_is_running = false;
    m_callback = nullptr;
    m_output_thread = nullptr;
    m_output_stream.clearReadStop();
    m_output_stream.clearWriteStop();
}

Audio_Player_Stream::~Audio_Player_Stream() {
    m_output_stream.stopReader();
    m_output_stream.stopWriter();
    auto lock = std::unique_lock(m_mutex_callback);
    m_is_running = false;
    if (m_output_thread != nullptr) {
        m_output_thread->join();
    }
};

void Audio_Player_Stream::set_callback(AudioPipelineSink::Callback callback) {
    auto lock = std::unique_lock(m_mutex_callback);
    m_is_running = false;
    if (m_output_thread != nullptr) {
        m_output_thread->join();
    }
    m_callback = callback;
    m_output_thread = nullptr;
    if (m_callback == nullptr) return;
    m_is_running = true;
    m_output_thread = std::make_unique<std::thread>([this, callback]() {
        while (m_is_running) {
            const size_t block_size = size_t(m_sample_rate*m_block_size_seconds);
            auto wr_buf = m_output_stream.writeBuf; // default buffer size is 1 million (we can avoid resizing)
            static_assert(sizeof(Frame<float>) == sizeof(dsp::stereo_t));
            auto frame_buf = tcb::span(reinterpret_cast<Frame<float>*>(wr_buf), block_size);
            const size_t total_written = callback(frame_buf, m_sample_rate); 
            bool is_buffer_swapped = false;
            if (total_written > 0) {
                is_buffer_swapped = m_output_stream.swap(int(total_written));
            }
            // @fix(#9): https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin/issues/9
            // buffer may not be swapped due to the following
            // - callback did not write any samples to buffer and is not blocking
            // - buffer swap failed because the writer was blocked which can occur if
            //   there is no reader and write operations have been disabled 
            // so we sleep here to avoid looping with zero blocking and consuming cpu cycles
            if (!is_buffer_swapped) {
                const int64_t sleep_ms = int64_t(m_block_size_seconds*1e3f);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
        }
    });
}

DABModule::DABModule(std::string _name) 
{
    name = _name;
    is_enabled = false;
    vfo = nullptr;
 
    // setup radio
    radio_block = std::make_unique<Radio_Block>(1,1);
    ofdm_demodulator_sink = std::make_unique<OFDM_Demodulator_Sink>(radio_block->get_ofdm_demodulator());
    ofdm_demodulator_sink->init(nullptr);
    radio_view_controller = std::make_unique<Radio_View_Controller>();
    // setup audio
    const float DEFAULT_AUDIO_SAMPLE_RATE = 48000.0f;
    auto audio_player_stream = std::make_unique<Audio_Player_Stream>(DEFAULT_AUDIO_SAMPLE_RATE, 0.1f);
    ev_handler_sample_rate_change.ctx = audio_player_stream.get();
    ev_handler_sample_rate_change.handler = [](float sample_rate, void* ctx) {
        auto* stream = reinterpret_cast<Audio_Player_Stream*>(ctx);
        stream->set_sample_rate(sample_rate);
    };
    audio_stream.init(&audio_player_stream->get_output_stream(), &ev_handler_sample_rate_change, DEFAULT_AUDIO_SAMPLE_RATE);
    audio_stream.setVolume(1.0f);
    sigpath::sinkManager.registerStream(name, &audio_stream);
    audio_stream.start();
    auto audio_pipeline = radio_block->get_audio_pipeline();
    audio_pipeline->set_sink(std::move(audio_player_stream));
    // setup gui
    gui::menu.registerEntry(name, [](void *ctx) {
        auto* mod = reinterpret_cast<DABModule*>(ctx);
        mod->RenderMenu();
    }, this, this);

    // Load configuration
    config.acquire();
    bool is_modified = false;
    if (!config.conf.contains("is_enabled")) {
        config.conf["is_enabled"] = false;
        is_modified = true;
    }
    const bool cfg_is_enabled = config.conf["is_enabled"];
    config.release(is_modified);
    if (cfg_is_enabled) {
        enable();
    }
}

DABModule::~DABModule() {
    audio_stream.stop();
    if (isEnabled()) {
        ofdm_demodulator_sink->stop();
        sigpath::vfoManager.deleteVFO(vfo);
    }
    sigpath::sinkManager.unregisterStream(name);
}

void DABModule::enable() { 
    is_enabled = true; 
    if (vfo == nullptr) {
        // NOTE: Use the entire 2.048e6 frequency range so that if we have a large
        //       frequency offset the VFO doesn't low pass filter out subcarriers
        const float MIN_BANDWIDTH = 2.048e6f;
        vfo = sigpath::vfoManager.createVFO(
            name, ImGui::WaterfallVFO::REF_CENTER,
            0, MIN_BANDWIDTH, MIN_BANDWIDTH, MIN_BANDWIDTH, MIN_BANDWIDTH, true);
        ofdm_demodulator_sink->setInput(vfo->output);
        ofdm_demodulator_sink->start();
    }

    config.acquire();
    config.conf["is_enabled"] = true;
    config.release(true);
}
void DABModule::disable() { 
    is_enabled = false; 
    if (vfo != nullptr) {
        ofdm_demodulator_sink->stop();
        sigpath::vfoManager.deleteVFO(vfo);
        vfo = nullptr;
    }

    config.acquire();
    config.conf["is_enabled"] = false;
    config.release(true);
}

void DABModule::RenderMenu() {
    const bool is_disabled = !is_enabled;
    if (is_disabled) style::beginDisabled();
    Render_Radio_Block(*radio_block, *radio_view_controller);
    if (is_disabled) style::endDisabled();
}