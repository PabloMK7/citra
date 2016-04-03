#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    export CC=gcc-5
    export CXX=g++-5
    mkdir -p $HOME/.local

    curl -L http://www.cmake.org/files/v2.8/cmake-2.8.11-Linux-i386.tar.gz \
        | tar -xz -C $HOME/.local --strip-components=1

    (
        wget http://libsdl.org/release/SDL2-2.0.4.tar.gz -O - | tar xz
        cd SDL2-2.0.4
        ./configure --prefix=$HOME/.local
        make -j4 && make install
    )
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew update > /dev/null # silence the very verbose output
    brew install qt5 sdl2 dylibbundler
    gem install xcpretty
fi
