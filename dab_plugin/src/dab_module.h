#pragma once

#include <complex>
#include <string>

#include <core.h>
#include <module.h>
#include <config.h>
#include <signal_path/vfo_manager.h>
#include <signal_path/sink.h>
#include <dsp/sink.h>
#include <dsp/stream.h>
#include <dsp/multirate/rational_resampler.h>

#include "audio/frame.h"
#include "utility/span.h"

extern ConfigManager config;

class DAB_Decoder;
class AudioPlayer;

class DisabledScoped 
{
private:
    bool is_disabled;
public:
    DisabledScoped(bool _is_disabled);
    ~DisabledScoped();
};

class DAB_Decoder_Sink: public dsp::Sink<dsp::complex_t>
{
private:
    using base_type = dsp::Sink<dsp::complex_t>;
    std::shared_ptr<DAB_Decoder> decoder;
public:
    DAB_Decoder_Sink(std::shared_ptr<DAB_Decoder> _decoder); 
    ~DAB_Decoder_Sink(); 
    int run(); 
private:
    void Process(const std::complex<float>* x, const int N);
public:
    auto& GetDecoder() { return *(decoder.get()); }
};

class Audio_Player_Stream
{
private:
    dsp::stream<dsp::stereo_t> output_stream;
public:
    Audio_Player_Stream(AudioPlayer& audio_player);
    ~Audio_Player_Stream();
public:
    auto& GetOutputStream() { return output_stream; }
private:
    void Process(tcb::span<const Frame<float>> rd_buf);
};

class DABModule: public ModuleManager::Instance 
{
private:
    std::shared_ptr<DAB_Decoder> dab_decoder;

    std::string name;
    bool is_enabled;
    std::unique_ptr<DAB_Decoder_Sink> decoder_sink;
    std::unique_ptr<Audio_Player_Stream> audio_player_stream;
    VFOManager::VFO* vfo;
    dsp::multirate::RationalResampler<dsp::stereo_t> audio_resampler;
    SinkManager::Stream audio_stream;
    EventHandler<float> ev_handler_sample_rate_change;
public:
    DABModule(std::string _name); 
    ~DABModule();

    void postInit();
    void enable(); 
    void disable(); 
    bool isEnabled(); 
private:
    void RenderMenu(); 
    void DrawConstellation(tcb::span<const std::complex<float>> data);
    void OnSampleRateChange(float new_sample_rate);
};