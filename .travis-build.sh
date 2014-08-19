#!/bin/sh

set -e

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = linux -o -z "$TRAVIS_OS_NAME" ]; then 
    mkdir build && cd build
    cmake -DUSE_QT5=OFF .. 
    make -j4
elif [ "$TRAVIS_OS_NAME" = osx ]; then
    mkdir build && cd build
    cmake .. -GXcode
    xcodebuild
fi
