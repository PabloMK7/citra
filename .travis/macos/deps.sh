#!/bin/sh -ex

brew update
brew install qt5 sdl2 p7zip ccache ffmpeg ninja
pip3 install macpack
