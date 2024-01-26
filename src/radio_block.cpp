#include "./radio_block.h"
#include "dab/constants/dab_parameters.h"
#include "ofdm/dab_mapper_ref.h"
#include "ofdm/dab_ofdm_params_ref.h"
#include "ofdm/dab_prs_ref.h"
#include "utility/span.h"

constexpr int TRANSMISSION_MODE = 1;

Radio_Block::Radio_Block(size_t ofdm_total_threads, size_t dab_total_threads) 
: m_ofdm_params(get_DAB_OFDM_params(TRANSMISSION_MODE)),
  m_dab_params(get_dab_parameters(TRANSMISSION_MODE)),
  m_ofdm_total_threads(ofdm_total_threads),
  m_dab_total_threads(dab_total_threads)
{
    // setup ofdm
    auto ofdm_prs_ref = std::vector<std::complex<float>>(m_ofdm_params.nb_fft);
    get_DAB_PRS_reference(TRANSMISSION_MODE, ofdm_prs_ref);
    auto ofdm_mapper_ref = std::vector<int>(m_ofdm_params.nb_data_carriers);
    get_DAB_mapper_ref(ofdm_mapper_ref, m_ofdm_params.nb_fft);
    m_ofdm_demodulator = std::make_shared<OFDM_Demod>(m_ofdm_params, ofdm_prs_ref, ofdm_mapper_ref, int(m_ofdm_total_threads));
    auto ring_buffer = std::make_shared<ThreadedRingBuffer<viterbi_bit_t>>(m_dab_params.nb_frame_bits*2);
    m_ofdm_to_radio_buffer = ring_buffer;
    m_ofdm_demodulator->On_OFDM_Frame().Attach([ring_buffer](tcb::span<const viterbi_bit_t> buf) {
        const size_t length = ring_buffer->write(buf);
        if (length != buf.size()) return;
    });

    // setup radio thread
    m_is_radio_thread_running = true;
    m_basic_radio = nullptr;
    m_thread_radio = std::make_unique<std::thread>([this]() {
        auto data = std::vector<viterbi_bit_t>(m_dab_params.nb_frame_bits);
        while (m_is_radio_thread_running) {
            const size_t length = m_ofdm_to_radio_buffer->read(data);
            if (length != data.size()) break;
            auto lock = std::scoped_lock(m_mutex_basic_radio);
            if (m_basic_radio == nullptr) continue;
            m_basic_radio->Process(data);
        }
    });
    // setup audio
    m_audio_pipeline = std::make_shared<AudioPipeline>();
    // setup radio
    reset_radio();
}

Radio_Block::~Radio_Block() {
    m_is_radio_thread_running = false;
    m_ofdm_to_radio_buffer->close();
    m_thread_radio->join();
}

void Radio_Block::reset_radio() {
    auto lock_audio = std::scoped_lock(m_mutex_audio_pipeline);
    auto audio_pipeline = m_audio_pipeline;
    auto radio = std::make_shared<BasicRadio>(m_dab_params, m_dab_total_threads);
    audio_pipeline->clear_sources();
    radio->On_DAB_Plus_Channel().Attach(
        [audio_pipeline](subchannel_id_t subchannel_id, Basic_DAB_Plus_Channel& channel) {
            auto& controls = channel.GetControls();
            auto audio_source = std::make_shared<AudioPipelineSource>();
            audio_pipeline->add_source(audio_source);
            channel.OnAudioData().Attach(
                [&controls, audio_source]
                (BasicAudioParams params, tcb::span<const uint8_t> buf) {
                    if (!controls.GetIsPlayAudio()) return;
                    auto frame_ptr = reinterpret_cast<const Frame<int16_t>*>(buf.data());
                    const size_t total_frames = buf.size() / sizeof(Frame<int16_t>);
                    auto frame_buf = tcb::span(frame_ptr, total_frames);
                    audio_source->write(frame_buf, float(params.frequency), false);
                }
            );
        }
    );
    auto lock_radio = std::scoped_lock(m_mutex_basic_radio);
    m_basic_radio = radio;
}

