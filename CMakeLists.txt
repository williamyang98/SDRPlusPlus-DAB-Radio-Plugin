cmake_minimum_required(VERSION 3.13)
project(sdrpp_plugin)

# platform compiler options
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_SILENCE_NONFLOATING_COMPLEX_DEPRECATION_WARNING)
    # for msvc multi processor compilation
    add_compile_options(/MP)
else()
    # NOTE: Compile with -fPIC on linux since static variables need to be relocatable for shared object
    add_compile_options(-fPIC)
endif()
# for posix threads
if(NOT DEFINED WIN32) 
    find_package(Threads REQUIRED)
    add_compile_options(-pthread) 
    link_libraries(Threads::Threads)
endif()


set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")


# build sdrpp_core
# sdrpp_core imports libraries using vcpkg or pkgconfig
# refer to ./toolchains/ubuntu/install_packages.sh or ./vcpkg.json for dependencies
if(MSVC)
    set(USE_INTERNAL_LIBCORRECT OFF CACHE BOOL "Use internal libcorrect")
    # we need to use our version of libcorrect due to ninja breaking on windows
    # this is because static and dynamic versions of the same library have the same name 
    # however on linux the output filenames are different so there isn't a conflict
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/libcorrect)
    # MSVC requires building from source
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/volk)
else()
    set(USE_INTERNAL_LIBCORRECT ON CACHE BOOL "Use internal libcorrect")
    # we need to use the internal library because sdrpp_core defaults to pkg-config
    # however pkg-config cant find libcorrect because it is not a package in the repository
endif()
set(OPT_BACKEND_GLFW ON CACHE BOOL "Enable glfw backend")
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/sdrplusplus/core)
set_target_properties(sdrpp_core PROPERTIES CXX_STANDARD 17)


# build dab modules
# copied from ./vendor/DAB-Radio/CMakeLists.txt 
set(DAB_RADIO_DIR ${CMAKE_SOURCE_DIR}/vendor/DAB-Radio)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${DAB_RADIO_DIR}/cmake")
find_package(faad2 REQUIRED)
find_package(mpg123 REQUIRED)
find_package(fmt REQUIRED)
set(viterbi_DIR ${DAB_RADIO_DIR}/vendor/viterbi_decoder)
find_package(viterbi CONFIG REQUIRED)
if(MSVC)
    # vcpkg
    find_package(FFTW3f CONFIG REQUIRED)
    set(FFTW3_LIBS FFTW3::fftw3f)
else()
    # pkgconfig
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(fftw3f REQUIRED IMPORTED_TARGET fftw3f)
    set(FFTW3_LIBS PkgConfig::fftw3f)
endif()
# build dab modules
add_subdirectory(${DAB_RADIO_DIR}/src/ofdm)
add_subdirectory(${DAB_RADIO_DIR}/src/dab)
add_subdirectory(${DAB_RADIO_DIR}/src/basic_radio)
# build dab plugin
find_package(audio_mixer REQUIRED)
add_subdirectory(${CMAKE_SOURCE_DIR}/src)
