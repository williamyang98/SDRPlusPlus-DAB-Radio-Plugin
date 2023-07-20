#!/bin/sh
TOOLCHAIN_FILE="C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"

cmake\
 -B build\
 -G Ninja\
 -DCMAKE_BUILD_TYPE=Release\
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE\
 -DCMAKE_TOOLCHAIN_FILE:STIRNG=$TOOLCHAIN_FILE
