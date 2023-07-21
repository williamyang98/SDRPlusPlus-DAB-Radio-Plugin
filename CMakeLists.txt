cmake_minimum_required(VERSION 3.13)
project(sdrpp_plugin)

# platform compiler options
if(DEFINED MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_SILENCE_NONFLOATING_COMPLEX_DEPRECATION_WARNING)
    # for msvc multi processor compilation
    add_compile_options(/MP)
endif()
# for posix threads
if(NOT DEFINED WIN32) 
    find_package(Threads REQUIRED)
    add_compile_options(-pthread) 
    link_libraries(Threads::Threads)
endif()


set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(volk REQUIRED)
# for dab plugin
find_package(audio_mixer REQUIRED)

# vcpkg
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
# for dab modules
find_package(FFTW3f CONFIG REQUIRED)
set(FFTW3_LIBS FFTW3::fftw3f)

# compile sdrplusplus core module for plugin building
set(OPT_BACKEND_GLFW ON CACHE BOOL "Enable glfw backend")
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/sdrplusplus/core)
set_target_properties(sdrpp_core PROPERTIES CXX_STANDARD 17)


# include libraries for dab modules
set(DAB_RADIO_DIR ${CMAKE_SOURCE_DIR}/vendor/DAB-Radio)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${DAB_RADIO_DIR}/cmake")
find_package(faad2 REQUIRED)
find_package(easyloggingpp REQUIRED)
find_package(fmt REQUIRED)
set(viterbi_DIR ${DAB_RADIO_DIR}/vendor/viterbi_decoder)
find_package(viterbi CONFIG REQUIRED)
# build dab modules
add_subdirectory(${DAB_RADIO_DIR}/src/ofdm)
add_subdirectory(${DAB_RADIO_DIR}/src/dab)
add_subdirectory(${DAB_RADIO_DIR}/src/basic_radio)
# simd and math compile options
if(NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    if(DEFINED MSVC)
        # NOTE: If your CPU doesn't support these instructions, modify /arch:?
        target_compile_options(ofdm_core PRIVATE /fp:fast /arch:AVX2)
        target_compile_options(dab_core PRIVATE /fp:fast /arch:AVX2)
    else()
        # NOTE: We are optimising specifically for your CPU's microarchitecture
        #       This is okay since we don't ship prebuilt binaries for linux distributions
        target_compile_options(ofdm_core PRIVATE -march=native -ffast-math)
        target_compile_options(dab_core PRIVATE -march=native -ffast-math)
    endif()
else()
    target_compile_options(ofdm_core PRIVATE -ffast-math)
    target_compile_options(dab_core PRIVATE -ffast-math)
endif()


# build dab plugin
add_subdirectory(${CMAKE_SOURCE_DIR}/src)

