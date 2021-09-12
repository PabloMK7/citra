#!/bin/sh -ex

brew update
brew unlink python@2 || true
rm '/usr/local/bin/2to3' || true
brew install qt5 p7zip ccache ninja || true
pip3 install macpack

export SDL_VER=2.0.16
export FFMPEG_VER=4.4

mkdir tmp
cd tmp/

# install SDL
wget https://github.com/SachinVin/ext-macos-bin/raw/main/sdl2/sdl-${SDL_VER}.7z
7z x sdl-${SDL_VER}.7z
cp -rv $(pwd)/sdl-${SDL_VER}/* /

# install FFMPEG
wget https://github.com/SachinVin/ext-macos-bin/raw/main/ffmpeg/ffmpeg-${FFMPEG_VER}.7z
7z x ffmpeg-${FFMPEG_VER}.7z
cp -rv $(pwd)/ffmpeg-${FFMPEG_VER}/* /
