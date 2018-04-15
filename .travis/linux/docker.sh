#!/bin/bash -ex

cd /citra

apt-get update
apt-get install -y build-essential libsdl2-dev qtbase5-dev libqt5opengl5-dev qttools5-dev qttools5-dev-tools wget git ccache

# Get a recent version of CMake
wget https://cmake.org/files/v3.10/cmake-3.10.1-Linux-x86_64.sh
echo y | sh cmake-3.10.1-Linux-x86_64.sh --prefix=cmake
export PATH=/citra/cmake/cmake-3.10.1-Linux-x86_64/bin:$PATH

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON
make -j4

ctest -VV -C Release
