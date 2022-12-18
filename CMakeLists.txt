cmake_minimum_required(VERSION 3.13)
project(sdrpp_plugin)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(faad2 REQUIRED)
find_package(easyloggingpp REQUIRED)
find_package(fmt REQUIRED)

find_package(FFTW3f CONFIG REQUIRED)

set(OPT_BACKEND_GLFW ON CACHE BOOL "Enable glfw backend")
add_subdirectory(${CMAKE_SOURCE_DIR}/core)

add_subdirectory(${CMAKE_SOURCE_DIR}/dab)
add_subdirectory(${CMAKE_SOURCE_DIR}/dab_plugin)

