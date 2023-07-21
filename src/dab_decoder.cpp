#include "./dab_decoder.h"
#include "ofdm/ofdm_helpers.h"

#include <algorithm>
#include <stdio.h>

DAB_Decoder::DAB_Decoder(const int transmission_mode, const int nb_threads) 
: is_reset_radio(false),
  dab_parameters(get_dab_parameters(transmission_mode))
{
    CreateBasicRadio();
    ofdm_demod = Create_OFDM_Demodulator(transmission_mode, nb_threads);
    audio_player = std::make_unique<AudioPlayer>();
    ofdm_output_double_buffer = std::make_unique<DoubleBuffer<viterbi_bit_t>>(dab_parameters.nb_frame_bits);

    // NOTE: We usually introduce a large frequency error when we are just eyeballing the spectrum
    auto& config = ofdm_demod->GetConfig();
    config.sync.max_coarse_freq_correction = 300000;

    ofdm_demod->On_OFDM_Frame().Attach([this](tcb::span<const viterbi_bit_t> data) {
        auto* buf = ofdm_output_double_buffer->AcquireInactiveBuffer();
        if (buf == NULL) return;

        const size_t N = ofdm_output_double_buffer->GetLength();
        std::copy_n(data.begin(), N, buf);
        ofdm_output_double_buffer->ReleaseInactiveBuffer();
    });
    
    radio_thread = std::make_unique<std::thread>([this]() {
        while (true) {
            if (is_reset_radio) {
                is_reset_radio = false;
                auto lock = std::unique_lock(mutex_radio_instance);
                CreateBasicRadio();
            }

            auto* buf = ofdm_output_double_buffer->AcquireActiveBuffer();
            if (buf == NULL) break;
            const size_t N = ofdm_output_double_buffer->GetLength();
            radio->Process({ buf, N });
            ofdm_output_double_buffer->ReleaseActiveBuffer();
        }
    });

}

void DAB_Decoder::CreateBasicRadio() {
    radio = std::make_unique<BasicRadio>(dab_parameters);

    // connect channel to audio mixers
    radio->On_DAB_Plus_Channel().Attach([this](subchannel_id_t subchannel_id, Basic_DAB_Plus_Channel& channel) {
        auto& controls = channel.GetControls(); 

        auto& audio_mixer = audio_player->GetMixer();
        auto lock_channel_mixers = std::unique_lock(mutex_channel_mixers);
        auto res = channel_mixers.find(subchannel_id);
        if (res == channel_mixers.end()) {
            const int ring_buffer_length = 2;
            auto buf = audio_mixer.CreateManagedBuffer(ring_buffer_length);
            auto player = std::make_unique<Resampled_PCM_Player>(buf, audio_player->GetSampleRate());
            res = channel_mixers.insert({ subchannel_id, std::move(player) }).first;
        }
        auto& player = res->second;
        lock_channel_mixers.unlock();

        channel.OnAudioData().Attach(
            [this, &controls, &player](BasicAudioParams params, tcb::span<const uint8_t> buf) {
                if (!controls.GetIsPlayAudio()) {
                    return;
                }
                auto rd_buf = tcb::span<const Frame<int16_t>>( 
                    reinterpret_cast<const Frame<int16_t>*>(buf.data()),
                    size_t(buf.size() / sizeof(Frame<int16_t>))
                );
                player->SetInputSampleRate(params.frequency);
                player->ConsumeBuffer(rd_buf);
            }
        );
    });
}

DAB_Decoder::~DAB_Decoder() {
    ofdm_output_double_buffer->Close();
    radio_thread->join();
};

void DAB_Decoder::Process(tcb::span<const std::complex<float>> x) {
    ofdm_demod->Process(x);
}
