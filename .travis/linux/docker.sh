#!/bin/bash -ex

cd /citra

apt-get update
apt-get install -y build-essential libsdl2-dev qtbase5-dev libqt5opengl5-dev libcurl4-openssl-dev libssl-dev wget git

# Get a recent version of CMake
wget https://cmake.org/files/v3.9/cmake-3.9.0-Linux-x86_64.sh
echo y | sh cmake-3.9.0-Linux-x86_64.sh --prefix=cmake
export PATH=/citra/cmake/cmake-3.9.0-Linux-x86_64/bin:$PATH

mkdir build && cd build
cmake .. -DUSE_SYSTEM_CURL=ON -DCMAKE_BUILD_TYPE=Release
make -j4

ctest -VV -C Release
