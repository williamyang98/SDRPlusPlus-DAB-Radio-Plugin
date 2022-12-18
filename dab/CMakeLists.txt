cmake_minimum_required(VERSION 3.13)
project(dab_radio)

# find_package(faad2 REQUIRED)
# find_package(easyloggingpp REQUIRED)
# find_package(fmt REQUIRED)
# find_package(FFTW3f CONFIG REQUIRED)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(OFDM_DIR ${SRC_DIR}/modules/ofdm)
add_library(ofdm_core STATIC 
    ${OFDM_DIR}/dab_mapper_ref.cpp
    ${OFDM_DIR}/dab_ofdm_params_ref.cpp
    ${OFDM_DIR}/dab_prs_ref.cpp
    ${OFDM_DIR}/ofdm_demodulator_threads.cpp
    ${OFDM_DIR}/ofdm_demodulator.cpp
    ${OFDM_DIR}/ofdm_modulator.cpp
    ${OFDM_DIR}/dsp/apply_pll.cpp
    ${OFDM_DIR}/dsp/complex_conj_mul_sum.cpp
)
set_target_properties(ofdm_core PROPERTIES CXX_STANDARD 17)
target_include_directories(ofdm_core PUBLIC ${OFDM_DIR} ${SRC_DIR})
target_link_libraries(ofdm_core PRIVATE FFTW3::fftw3f fmt)
target_compile_options(ofdm_core PRIVATE /fp:fast /arch:AVX2)

set(DAB_CORE_DIR ${SRC_DIR}/modules/dab)
add_library(dab_core STATIC
    ${DAB_CORE_DIR}/algorithms/viterbi_decoder.cpp
    ${DAB_CORE_DIR}/algorithms/phil_karn_viterbi_decoder.cpp
    ${DAB_CORE_DIR}/algorithms/reed_solomon_decoder.cpp
    ${DAB_CORE_DIR}/fic/fic_decoder.cpp
    ${DAB_CORE_DIR}/fic/fig_processor.cpp
    ${DAB_CORE_DIR}/database/dab_database.cpp
    ${DAB_CORE_DIR}/database/dab_database_updater.cpp
    ${DAB_CORE_DIR}/msc/msc_decoder.cpp
    ${DAB_CORE_DIR}/msc/cif_deinterleaver.cpp
    ${DAB_CORE_DIR}/msc/msc_xpad_processor.cpp
    ${DAB_CORE_DIR}/audio/aac_frame_processor.cpp
    ${DAB_CORE_DIR}/audio/aac_audio_decoder.cpp
    ${DAB_CORE_DIR}/audio/aac_data_decoder.cpp
    ${DAB_CORE_DIR}/mot/MOT_assembler.cpp
    ${DAB_CORE_DIR}/mot/MOT_processor.cpp
    ${DAB_CORE_DIR}/mot/MOT_slideshow_processor.cpp
    ${DAB_CORE_DIR}/pad/pad_data_group.cpp
    ${DAB_CORE_DIR}/pad/pad_data_length_indicator.cpp
    ${DAB_CORE_DIR}/pad/pad_dynamic_label_assembler.cpp
    ${DAB_CORE_DIR}/pad/pad_dynamic_label.cpp
    ${DAB_CORE_DIR}/pad/pad_MOT_processor.cpp
    ${DAB_CORE_DIR}/pad/pad_processor.cpp
    ${DAB_CORE_DIR}/radio_fig_handler.cpp)
set_target_properties(dab_core PROPERTIES CXX_STANDARD 17)
target_include_directories(dab_core PUBLIC ${DAB_CORE_DIR} ${SRC_DIR})
target_link_libraries(dab_core PRIVATE faad2 easyloggingpp fmt)
target_compile_definitions(dab_core PRIVATE ELPP_THREAD_SAFE)

set(BASIC_RADIO_DIR ${SRC_DIR}/modules/basic_radio)
add_library(basic_radio STATIC
    ${BASIC_RADIO_DIR}/basic_radio.cpp
    ${BASIC_RADIO_DIR}/basic_fic_runner.cpp
    ${BASIC_RADIO_DIR}/basic_dab_plus_channel.cpp
    ${BASIC_RADIO_DIR}/basic_slideshow.cpp
    ${BASIC_RADIO_DIR}/basic_database_manager.cpp)
set_target_properties(basic_radio PROPERTIES CXX_STANDARD 17)
target_include_directories(basic_radio PUBLIC ${BASIC_RADIO_DIR} ${SRC_DIR})
target_link_libraries(basic_radio PRIVATE dab_core easyloggingpp fmt)
target_compile_definitions(basic_radio PRIVATE ELPP_THREAD_SAFE)

set(BASIC_SCRAPER_DIR ${SRC_DIR}/modules/basic_scraper)
add_library(basic_scraper STATIC
    ${BASIC_SCRAPER_DIR}/basic_scraper.cpp)
set_target_properties(basic_scraper PROPERTIES CXX_STANDARD 17)
target_include_directories(basic_scraper PUBLIC ${BASIC_SCRAPER_DIR} ${SRC_DIR})
target_link_libraries(basic_scraper PRIVATE basic_radio easyloggingpp fmt)
target_compile_definitions(basic_scraper PRIVATE ELPP_THREAD_SAFE)

set(AUDIO_LIB_DIR ${SRC_DIR}/audio)
add_library(audio_lib STATIC
    ${AUDIO_LIB_DIR}/audio_mixer.cpp
    ${AUDIO_LIB_DIR}/resampled_pcm_player.cpp)
set_target_properties(audio_lib PROPERTIES CXX_STANDARD 17)
target_include_directories(audio_lib PUBLIC ${AUDIO_LIB_DIR} ${SRC_DIR})

add_library(getopt STATIC
    ${SRC_DIR}/utility/getopt/getopt.c)
set_target_properties(getopt PROPERTIES CXX_STANDARD 17)
target_include_directories(getopt PUBLIC ${SRC_DIR} ${SRC_DIR}/utility/getopt)