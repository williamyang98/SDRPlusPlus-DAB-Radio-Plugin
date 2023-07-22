#!/bin/sh
BUILD_DIR="build-ubuntu"
BUILD_TARGET="dab_plugin"

ninja -C $BUILD_DIR $BUILD_TARGET
