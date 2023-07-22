cmake_minimum_required(VERSION 3.10)
project(libcorrect)

set(LIBCORRECT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/libcorrect)
add_subdirectory(${LIBCORRECT_SRC_DIR})
