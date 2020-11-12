#!/bin/sh -ex

brew update
brew unlink python@2 || true
brew install qt5 sdl2 p7zip ccache ffmpeg llvm ninja
pip3 install macpack
