#pragma once

#include <memory>
#include <complex>
#include <unordered_map>
#include <thread>
#include "utility/span.h"
#include "utility/double_buffer.h"

#include "audio_player.h"
#include "modules/ofdm/ofdm_demodulator.h"
#include "modules/basic_radio/basic_radio.h"
#include "audio/resampled_pcm_player.h"

class DAB_Decoder 
{
private:
    std::unique_ptr<OFDM_Demod> ofdm_demod;
    std::unique_ptr<BasicRadio> radio;
    std::unique_ptr<AudioPlayer> audio_player;
    std::unordered_map<subchannel_id_t, std::unique_ptr<Resampled_PCM_Player>> dab_plus_audio_players;

    std::unique_ptr<DoubleBuffer<viterbi_bit_t>> ofdm_output_double_buffer;
    std::unique_ptr<std::thread> radio_thread;
public:
    DAB_Decoder(const int transmission_mode, const int nb_threads=0);
    ~DAB_Decoder();
    void Process(tcb::span<const std::complex<float>> x);
    auto& OnAudioBlock() { return audio_player->OnBlock(); }
public:
    auto& GetOFDMDemodulator() { return *(ofdm_demod.get()); }
    auto& GetBasicRadio() { return *(radio.get()); }
    auto& GetAudioPlayer() { return *(audio_player.get()); }
};