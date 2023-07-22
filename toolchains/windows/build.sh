#!/bin/sh
BUILD_DIR="build-windows"
BUILD_TARGET="dab_plugin"

ninja -C $BUILD_DIR $BUILD_TARGET
