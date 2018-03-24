#!/bin/bash -ex

cd /citra

apt-get update
apt-get install -y build-essential wget git python-launchpadlib

# Install specific versions of packages with their dependencies
# The apt repositories remove older versions regularly, so we can't use
# apt-get and have to pull the packages directly from the archives.
/citra/.travis/linux-frozen/install_package.py       \
    libsdl2-dev 2.0.7+dfsg1-3ubuntu1 bionic          \
    qtbase5-dev 5.9.3+dfsg-0ubuntu2 bionic           \
    libqt5opengl5-dev 5.9.3+dfsg-0ubuntu2 bionic     \
    libicu57 57.1-6ubuntu0.2 bionic

# Get a recent version of CMake
wget https://cmake.org/files/v3.10/cmake-3.10.1-Linux-x86_64.sh
echo y | sh cmake-3.10.1-Linux-x86_64.sh --prefix=cmake
export PATH=/citra/cmake/cmake-3.10.1-Linux-x86_64/bin:$PATH

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"}
make -j4

ctest -VV -C Release
