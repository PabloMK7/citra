#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    export CC=gcc-4.9
    export CXX=g++-4.9
    mkdir -p $HOME/.local

    curl http://www.cmake.org/files/v2.8/cmake-2.8.11-Linux-i386.tar.gz \
        | tar -xz -C $HOME/.local --strip-components=1

    (
        git clone https://github.com/glfw/glfw.git --branch 3.1.1 --depth 1
        mkdir glfw/build && cd glfw/build
        cmake -DBUILD_SHARED_LIBS=ON \
              -DGLFW_BUILD_EXAMPLES=OFF \
              -DGLFW_BUILD_TESTS=OFF \
              -DCMAKE_INSTALL_PREFIX=$HOME/.local \
              ..
        make -j4 && make install
    )

elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew update > /dev/null # silence the very verbose output
    brew install qt5 glfw3 pkgconfig
    gem install xcpretty
fi
