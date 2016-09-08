#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    export CC=gcc-6
    export CXX=g++-6
    mkdir -p $HOME/.local

    if [ ! -e $HOME/.local/bin/cmake ]; then
        echo "CMake not found in the cache, get and extract it..."
        curl -L http://www.cmake.org/files/v3.2/cmake-3.2.0-Linux-i386.tar.gz \
            | tar -xz -C $HOME/.local --strip-components=1
    else
        echo "Using cached CMake"
    fi

    if [ ! -e $HOME/.local/lib/libSDL2.la ]; then
        echo "SDL2 not found in cache, get and build it..."
        wget http://libsdl.org/release/SDL2-2.0.4.tar.gz -O - | tar xz
        cd SDL2-2.0.4
        ./configure --prefix=$HOME/.local
        make -j4 && make install
    else
        echo "Using cached SDL2"
    fi

elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew update > /dev/null # silence the very verbose output
    brew unlink cmake || true
    brew install cmake qt5 sdl2 dylibbundler
fi
