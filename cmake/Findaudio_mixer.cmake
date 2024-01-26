cmake_minimum_required(VERSION 3.10)
project(audio_mixer)

# extracts the audio mixing code from examples directory
set(DAB_RADIO_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/DAB-Radio)
set(SRC_DIR ${DAB_RADIO_DIR}/examples/audio)
set(ROOT_DIR ${DAB_RADIO_DIR}/src)

add_library(audio_mixer STATIC 
    ${SRC_DIR}/audio_pipeline.cpp)
set_target_properties(audio_mixer PROPERTIES CXX_STANDARD 17)
target_include_directories(audio_mixer PUBLIC ${SRC_DIR} ${ROOT_DIR})
