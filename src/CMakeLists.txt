cmake_minimum_required(VERSION 3.13)
project(dab_plugin)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR})
set(DAB_RADIO_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/DAB-Radio)
# use the utility/*.h header files
set(ROOT_DIR ${DAB_RADIO_DIR}/src)
# use the example/audio header files
set(AUDIO_DIR ${DAB_RADIO_DIR}/examples)

add_library(dab_plugin SHARED 
    # module files
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/dab_module.cpp
    # glue code
    ${SRC_DIR}/dab_decoder.cpp
    ${SRC_DIR}/audio_player.cpp
    ${SRC_DIR}/render_dab_module.cpp
    ${SRC_DIR}/render_formatters.cpp
    ${SRC_DIR}/texture.cpp
)
set_target_properties(dab_plugin PROPERTIES CXX_STANDARD 17)
target_include_directories(dab_plugin PRIVATE ${SRC_DIR} ${ROOT_DIR} ${AUDIO_DIR})
target_link_libraries(dab_plugin PRIVATE 
    sdrpp_core 
    ofdm_core dab_core basic_radio 
    audio_mixer
    easyloggingpp fmt)
target_compile_definitions(dab_plugin PRIVATE ELPP_THREAD_SAFE)