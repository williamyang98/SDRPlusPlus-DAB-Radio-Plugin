cmake_minimum_required(VERSION 3.13)
project(sdrpp_plugin)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(faad2 REQUIRED)
find_package(easyloggingpp REQUIRED)
find_package(fmt REQUIRED)
find_package(volk REQUIRED)

# vcpkg
find_package(OpenGL REQUIRED)
find_package(FFTW3f CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# compile sdr++ core module for plugin building
set(OPT_BACKEND_GLFW ON CACHE BOOL "Enable glfw backend")
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/sdrplusplus/core)
target_compile_features(sdrpp_core PRIVATE cxx_std_17)
# NOTE: nested-namespace-definition required for compile
target_compile_options(sdrpp_core PRIVATE /std:c++17)


# build dab modules
add_subdirectory(${CMAKE_SOURCE_DIR}/dab)
# compile dab and ofdm modules to use AVX2 for speedups
# NOTE: If your CPU doesn't suppor these instructions, modify this
target_compile_options(ofdm_core PRIVATE /fp:fast /arch:AVX)
target_compile_options(dab_core PRIVATE /fp:fast /arch:AVX)


# build dab plugin
add_subdirectory(${CMAKE_SOURCE_DIR}/dab_plugin)

