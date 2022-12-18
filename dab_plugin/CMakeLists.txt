cmake_minimum_required(VERSION 3.13)
project(dab_plugin)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(dab_plugin SHARED 
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/dab_module.cpp

    ${SRC_DIR}/dab_decoder.cpp
    ${SRC_DIR}/audio_player.cpp
    ${SRC_DIR}/render_dab_module.cpp
    ${SRC_DIR}/render_formatters.cpp
)
set_target_properties(dab_plugin PROPERTIES CXX_STANDARD 17)
target_include_directories(dab_plugin PRIVATE ${SRC_DIR})
target_link_libraries(dab_plugin PRIVATE 
    sdrpp_core 
    ofdm_core dab_core basic_radio audio_lib
    easyloggingpp fmt)
target_compile_definitions(dab_plugin PRIVATE ELPP_THREAD_SAFE)

if (MSVC)
    target_compile_options(dab_plugin PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(dab_plugin PRIVATE -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
else ()
    target_compile_options(dab_plugin PRIVATE -O3 -std=c++17)
endif ()