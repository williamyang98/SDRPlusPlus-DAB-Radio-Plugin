#pragma once

#include <memory>
#include <complex>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "./audio_player.h"
#include "utility/span.h"
#include "utility/double_buffer.h"
#include "audio/resampled_pcm_player.h"
#include "ofdm/ofdm_demodulator.h"
#include "ofdm/ofdm_params.h"
#include "basic_radio/basic_radio.h"

class DAB_Decoder 
{
private:
    const DAB_Parameters dab_parameters;
    std::unique_ptr<OFDM_Demod> ofdm_demod;
    std::unique_ptr<BasicRadio> radio;
    std::unique_ptr<AudioPlayer> audio_player;
    // audio mixer 
    std::unordered_map<subchannel_id_t, std::unique_ptr<Resampled_PCM_Player>> channel_mixers;
    std::mutex mutex_channel_mixers;
    // separate threads for ofdm demodulation and dab digital decoding
    std::unique_ptr<DoubleBuffer<viterbi_bit_t>> ofdm_output_double_buffer;
    std::unique_ptr<std::thread> radio_thread;
    // flag to reset the dab radio instance
    bool is_reset_radio;
    std::mutex mutex_radio_instance;
public:
    DAB_Decoder(const int transmission_mode, const int nb_threads=0);
    ~DAB_Decoder();
    void Process(tcb::span<const std::complex<float>> x);
    auto& OnAudioBlock() { return audio_player->OnBlock(); }
public:
    auto& GetOFDMDemodulator() { return *(ofdm_demod.get()); }
    auto& GetAudioPlayer() { return *(audio_player.get()); }
    auto& GetBasicRadio() { return *(radio.get()); }
    auto& GetBasicRadioMutex() { return mutex_radio_instance; }
    void RaiseResetBasicRadioFlag() { is_reset_radio = true; }
private:
    void CreateBasicRadio();
};