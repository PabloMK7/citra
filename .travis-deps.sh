#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
    sudo apt-get -qq update
    sudo apt-get -qq install g++-4.9 xorg-dev libglu1-mesa-dev libxcursor-dev
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.9 90
    (
        git clone https://github.com/glfw/glfw.git --branch 3.0.4 --depth 1
        mkdir glfw/build && cd glfw/build
        cmake -DBUILD_SHARED_LIBS=ON \
              -DGLFW_BUILD_EXAMPLES=OFF \
              -DGLFW_BUILD_TESTS=OFF \
              ..
        make -j4 && sudo make install
    )

    sudo apt-get install lib32stdc++6
    sudo mkdir -p /usr/local
    curl http://www.cmake.org/files/v2.8/cmake-2.8.11-Linux-i386.tar.gz \
        | sudo tar -xz -C /usr/local --strip-components=1
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew tap homebrew/versions
    brew install qt5 glfw3 pkgconfig
fi
