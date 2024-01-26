#pragma once

#include <mutex>
#include <stddef.h>
#include <memory>
#include "ofdm/ofdm_demodulator.h"
#include "basic_radio/basic_radio.h"
#include "audio/audio_pipeline.h"
#include "app_helpers/app_io_buffers.h"

class Radio_Block 
{
private:
    const OFDM_Params m_ofdm_params;
    const DAB_Parameters m_dab_params;
    const size_t m_ofdm_total_threads;
    const size_t m_dab_total_threads;
    std::shared_ptr<OFDM_Demod> m_ofdm_demodulator;
    std::shared_ptr<ThreadedRingBuffer<viterbi_bit_t>> m_ofdm_to_radio_buffer;
    std::unique_ptr<std::thread> m_thread_radio;
    std::mutex m_mutex_basic_radio;
    bool m_is_radio_thread_running;
    std::shared_ptr<BasicRadio> m_basic_radio;
    std::mutex m_mutex_audio_pipeline;
    std::shared_ptr<AudioPipeline> m_audio_pipeline;
public:
    Radio_Block(size_t ofdm_total_threads, size_t dab_total_threads);
    ~Radio_Block();
    void reset_radio();
    std::shared_ptr<OFDM_Demod> get_ofdm_demodulator() { return m_ofdm_demodulator; }
    std::shared_ptr<BasicRadio> get_basic_radio() { 
        auto lock = std::unique_lock(m_mutex_basic_radio);
        return m_basic_radio;
    }
    std::shared_ptr<AudioPipeline> get_audio_pipeline() { return m_audio_pipeline; }
};

