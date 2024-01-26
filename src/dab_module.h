#pragma once
#include <complex>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <core.h>
#include <module.h>
#include <config.h>
#include <signal_path/vfo_manager.h>
#include <signal_path/sink.h>
#include <dsp/sink.h>
#include <dsp/stream.h>
#include "audio/audio_pipeline.h"

extern ConfigManager config;

class OFDM_Demod;
class Radio_View_Controller;
class Radio_Block;

class OFDM_Demodulator_Sink: public dsp::Sink<dsp::complex_t>
{
private:
    using base_type = dsp::Sink<dsp::complex_t>;
    std::shared_ptr<OFDM_Demod> m_ofdm_demod;
public:
    OFDM_Demodulator_Sink(std::shared_ptr<OFDM_Demod> ofdm_demod): m_ofdm_demod(ofdm_demod) {}
    ~OFDM_Demodulator_Sink() override {
        if (!base_type::_block_init) return;
        base_type::stop();
    }
    int run();
};

class Audio_Player_Stream: public AudioPipelineSink
{
private:
    float m_sample_rate;
    float m_block_size_seconds;
    dsp::stream<dsp::stereo_t> m_output_stream;
    std::unique_ptr<std::thread> m_output_thread;
    AudioPipelineSink::Callback m_callback;
    std::mutex m_mutex_callback;
    bool m_is_running;
public:
    Audio_Player_Stream(float sample_rate, float block_size_seconds);
    ~Audio_Player_Stream() override;
    void set_callback(AudioPipelineSink::Callback callback) override;
    std::string_view get_name() const override { return "sdr_audio_sink"; }
public:
    auto& get_output_stream() { return m_output_stream; }
    void set_sample_rate(float sample_rate) { m_sample_rate = sample_rate; }
    void set_block_size_seconds(float block_size_seconds) { m_block_size_seconds = block_size_seconds; }
};

class DABModule: public ModuleManager::Instance 
{
private:
    std::unique_ptr<OFDM_Demodulator_Sink> ofdm_demodulator_sink;
    std::unique_ptr<Radio_Block> radio_block;
    std::unique_ptr<Radio_View_Controller> radio_view_controller;

    std::string name;
    bool is_enabled;
    VFOManager::VFO* vfo;
    SinkManager::Stream audio_stream;
    EventHandler<float> ev_handler_sample_rate_change;
public:
    DABModule(std::string _name); 
    ~DABModule();
    void postInit() override {}
    void enable() override; 
    void disable() override; 
    bool isEnabled() override { return is_enabled; }
private:
    void RenderMenu(); 
};