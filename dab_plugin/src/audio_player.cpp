#include "./audio_player.h"
#include <chrono>

AudioPlayer::AudioPlayer(const int _sample_rate)
: sample_rate(_sample_rate), total_channels(TOTAL_AUDIO_CHANNELS),
  frames_per_block(_sample_rate/10),
  mixer(frames_per_block)
{
    is_running = true;
    runner_thread = std::make_unique<std::thread>([this]() {
        RunnerThread();
    });
}

AudioPlayer::~AudioPlayer() {
    is_running = false;
    runner_thread->join();
};

void AudioPlayer::RunnerThread() {
    const float T_frame = 1.0f/(float)sample_rate;
    const float T_block = T_frame * (float)frames_per_block;
    const float T_block_ms = T_block * 1000.0f;
    const auto duration = std::chrono::milliseconds((long long)T_block_ms);

    while (is_running) {
        // std::this_thread::sleep_for(duration);
        // NOTE: Rely on the callback to set the duration

        auto rd_buf = mixer.UpdateMixer();
        if (frames_per_block != rd_buf.size()) {
            continue;            
        }

        obs_on_block.Notify(rd_buf);
    }
}