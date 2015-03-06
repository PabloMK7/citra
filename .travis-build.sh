#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    mkdir build && cd build
    cmake -DUSE_QT5=OFF .. 
    make -j4
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    export Qt5_DIR=$(brew --prefix)/opt/qt5
    mkdir build && cd build
    cmake .. -GXcode
    xcodebuild -configuration Release
fi
