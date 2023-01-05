#!/bin/sh -ex

brew install ccache ninja || true
pip3 install macpack

export FFMPEG_VER=5.1
export QT_VER=5.15.8

mkdir tmp
cd tmp/

# install FFMPEG
wget https://github.com/SachinVin/ext-macos-bin/raw/main/ffmpeg/ffmpeg-${FFMPEG_VER}.7z
7z x ffmpeg-${FFMPEG_VER}.7z
cp -rv $(pwd)/ffmpeg-${FFMPEG_VER}/* /

# install Qt
wget https://github.com/SachinVin/ext-macos-bin/raw/main/qt/qt-${QT_VER}.7z
7z x qt-${QT_VER}.7z
sudo cp -rv $(pwd)/qt-${QT_VER}/* /usr/local/
