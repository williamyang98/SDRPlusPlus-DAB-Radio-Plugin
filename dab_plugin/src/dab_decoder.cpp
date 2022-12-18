#include "dab_decoder.h"

#include "modules/ofdm/ofdm_helpers.h"
#include "modules/basic_radio/basic_radio.h"
#include <algorithm>

#include <stdio.h>

DAB_Decoder::DAB_Decoder(const int transmission_mode, const int nb_threads) {
    ofdm_demod = Create_OFDM_Demodulator(transmission_mode, nb_threads);
    auto params = get_dab_parameters(transmission_mode);
    radio = std::make_unique<BasicRadio>(params);
    audio_player = std::make_unique<AudioPlayer>();
    ofdm_output_double_buffer = std::make_unique<DoubleBuffer<viterbi_bit_t>>(params.nb_frame_bits);

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
            auto* buf = ofdm_output_double_buffer->AcquireActiveBuffer();
            if (buf == NULL) break;

            const size_t N = ofdm_output_double_buffer->GetLength();
            radio->Process({ buf, N });
            ofdm_output_double_buffer->ReleaseActiveBuffer();
        }
    });

    radio->On_DAB_Plus_Channel().Attach([this](subchannel_id_t subchannel_id, Basic_DAB_Plus_Channel& channel) {
        auto& controls = channel.GetControls(); 
        auto& mixer = audio_player->GetMixer();
        
        auto res = dab_plus_audio_players.find(subchannel_id);
        if (res == dab_plus_audio_players.end()) {
            const int ring_buffer_length = 4;
            auto buf = mixer.CreateManagedBuffer(ring_buffer_length);
            auto player = std::make_unique<Resampled_PCM_Player>(buf, audio_player->GetSampleRate());
            res = dab_plus_audio_players.insert({ subchannel_id, std::move(player) }).first;
        }

        auto& player = res->second;

        channel.OnAudioData().Attach([this, &controls, &player](BasicAudioParams params, tcb::span<const uint8_t> buf) {
            if (!controls.GetIsPlayAudio()) {
                return;
            }

            auto rd_buf = tcb::span<const Frame<int16_t>>( 
                reinterpret_cast<const Frame<int16_t>*>(buf.data()),
                (size_t)(buf.size() / sizeof(Frame<int16_t>))
            );

            player->SetInputSampleRate(params.frequency);
            player->ConsumeBuffer(rd_buf);
        });
        
    });
}

DAB_Decoder::~DAB_Decoder() {
    ofdm_output_double_buffer->Close();
    radio_thread->join();
};

void DAB_Decoder::Process(tcb::span<const std::complex<float>> x) {
    ofdm_demod->Process(x);
}