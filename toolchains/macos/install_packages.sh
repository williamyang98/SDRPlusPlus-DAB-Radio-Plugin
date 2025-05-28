#!/bin/sh
# install specific version of cmake 3.x.x since 4.x.x is not backwards compatible with < 3.5
# need to pass special flags and environment variables so brew treats local file as formula instead of a local cask or remote url
wget -O ./cmake.rb https://raw.githubusercontent.com/Homebrew/homebrew-core/14ea6eca0e0b0b4f76940f1f7a929869d5a49930/Formula/c/cmake.rb
HOMEBREW_NO_INSTALL_FROM_API=1 brew install --build-from-source ./cmake.rb
brew install fftw
brew install pkg-config
brew install glfw
brew install zstd
brew install ninja
