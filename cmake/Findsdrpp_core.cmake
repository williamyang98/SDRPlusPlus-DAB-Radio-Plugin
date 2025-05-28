# Copied from vendor/sdrplusplus/core/CMakeLists.txt
cmake_minimum_required(VERSION 3.13)
project(sdrpp_core)

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/sdrplusplus/core)

# Main code
file(GLOB_RECURSE SRC "${SRC_DIR}/src/*.cpp" "${SRC_DIR}/src/*.c")

if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

# Add code to dyn lib
add_library(sdrpp_core SHARED ${SRC})
# configure SDRPP_EXPORT dllimport/dllexport/extern properly for core/src/module.h
target_compile_definitions(sdrpp_core PRIVATE SDRPP_IS_CORE)

# Set compiler options
if (MSVC)
    target_compile_options(sdrpp_core PRIVATE /O2 /Ob2 /std:c++17 /EHsc)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(sdrpp_core PRIVATE -O3 -std=c++17)
else ()
    target_compile_options(sdrpp_core PRIVATE -O3 -std=c++17)
endif ()


# Set the install prefix
target_compile_definitions(sdrpp_core PUBLIC INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

# Include core headers
target_include_directories(sdrpp_core PUBLIC "${SRC_DIR}/src/")
target_include_directories(sdrpp_core PUBLIC "${SRC_DIR}/src/imgui")

# Link to linkcorrect
if (USE_INTERNAL_LIBCORRECT)
add_subdirectory("${SRC_DIR}/libcorrect/")
target_include_directories(sdrpp_core PUBLIC "${SRC_DIR}/libcorrect/include")
target_link_libraries(sdrpp_core PUBLIC correct_static)
endif (USE_INTERNAL_LIBCORRECT)

if (OPT_OVERRIDE_STD_FILESYSTEM)
target_include_directories(sdrpp_core PUBLIC "${SRC_DIR}/std_replacement")
endif (OPT_OVERRIDE_STD_FILESYSTEM)

if (MSVC)
    # Lib path
    target_link_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(sdrpp_core PUBLIC "C:/Program Files/PothosSDR/include/")

    # Volk
    target_link_libraries(sdrpp_core PUBLIC volk)

    # Glew
    find_package(GLEW REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC GLEW::GLEW)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC glfw)

    # FFTW3
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(sdrpp_core PUBLIC FFTW3::fftw3f)

    # WinSock2
    target_link_libraries(sdrpp_core PUBLIC wsock32 ws2_32)

else()
    find_package(PkgConfig)
    find_package(OpenGL REQUIRED)

    pkg_check_modules(GLEW REQUIRED glew)
    pkg_check_modules(FFTW3 REQUIRED fftw3f)
    pkg_check_modules(VOLK REQUIRED volk)
    pkg_check_modules(GLFW3 REQUIRED glfw3)

    target_include_directories(sdrpp_core PUBLIC
        ${GLEW_INCLUDE_DIRS}
        ${FFTW3_INCLUDE_DIRS}
        ${GLFW3_INCLUDE_DIRS}
        ${VOLK_INCLUDE_DIRS}
    )

    target_link_directories(sdrpp_core PUBLIC
        ${GLEW_LIBRARY_DIRS}
        ${FFTW3_LIBRARY_DIRS}
        ${GLFW3_LIBRARY_DIRS}
        ${VOLK_LIBRARY_DIRS}
    )

    target_link_libraries(sdrpp_core PUBLIC
        ${OPENGL_LIBRARIES}
        ${GLEW_LIBRARIES}
        ${FFTW3_LIBRARIES}
        ${GLFW3_LIBRARIES}
        ${VOLK_LIBRARIES}
    )

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(sdrpp_core PUBLIC stdc++fs)
    endif ()

    if (NOT USE_INTERNAL_LIBCORRECT)
        pkg_check_modules(CORRECT REQUIRED libcorrect)
        target_include_directories(sdrpp_core PUBLIC ${CORRECT_INCLUDE_DIRS})
        target_link_directories(sdrpp_core PUBLIC ${CORRECT_LIBRARY_DIRS})
        target_link_libraries(sdrpp_core PUBLIC ${CORRECT_LIBRARIES})
    endif (NOT USE_INTERNAL_LIBCORRECT)
endif ()
