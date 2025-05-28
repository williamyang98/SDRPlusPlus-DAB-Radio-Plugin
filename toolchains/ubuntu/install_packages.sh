#!/bin/sh

sudo apt-get --yes install build-essential ninja-build
sudo apt-get --yes install libglew-dev libglfw3-dev libopengl-dev libfftw3-dev libfftw3-single3 libvolk2-dev libzstd-dev libcpu-features-dev

# install mako for volk build
python3 -m pip install mako --quiet
