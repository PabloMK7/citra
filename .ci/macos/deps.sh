#!/bin/sh -ex

brew install ccache ninja || true

export QT_VER=5.15.8

mkdir tmp
cd tmp/

# install Qt
wget https://github.com/citra-emu/ext-macos-bin/raw/main/qt/qt-${QT_VER}.7z
7z x qt-${QT_VER}.7z
sudo cp -rv $(pwd)/qt-${QT_VER}/* /usr/local/
