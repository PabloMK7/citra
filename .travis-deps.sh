#!/bin/sh

set -e

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = linux -o -z "$TRAVIS_OS_NAME" ]; then
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
    sudo apt-get -qq update
    sudo apt-get -qq install g++-4.8 xorg-dev libglu1-mesa-dev libglew-dev libxcursor-dev
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 90
    git clone https://github.com/glfw/glfw.git
    mkdir glfw/build && cd glfw/build
    cmake .. && make && sudo make install 
    cd -	
elif [ "$TRAVIS_OS_NAME" = osx ]; then
    brew tap homebrew/versions
    brew install glew qt5 glfw3 pkgconfig
fi
