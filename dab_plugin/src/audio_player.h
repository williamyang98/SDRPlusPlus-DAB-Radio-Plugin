#pragma once

#include <thread>
#include <memory>
#include "utility/span.h"
#include "utility/observable.h"
#include "audio/audio_mixer.h"

class AudioPlayer 
{
private:
    const int frames_per_block;
    const int sample_rate;
    const int total_channels;
    AudioMixer mixer;
    bool is_running;
    std::unique_ptr<std::thread> runner_thread;
    Observable<tcb::span<const Frame<float>>> obs_on_block;
public:
    AudioPlayer(const int _sample_rate=48000);
    ~AudioPlayer();
    auto& GetMixer() { return mixer; }
    auto& OnBlock() { return obs_on_block; }
    int GetSampleRate() const { return sample_rate; }
private:
    void RunnerThread();
};