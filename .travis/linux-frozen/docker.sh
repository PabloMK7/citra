#!/bin/bash -ex

cd /citra

apt-get update
apt-get install -y build-essential wget git python-launchpadlib libssl-dev

# Install specific versions of packages with their dependencies
# The apt repositories remove older versions regularly, so we can't use
# apt-get and have to pull the packages directly from the archives.
/citra/.travis/linux-frozen/install_package.py       \
    libsdl2-dev 2.0.4+dfsg1-2ubuntu2 xenial          \
    qtbase5-dev 5.2.1+dfsg-1ubuntu14.3 trusty        \
    libqt5opengl5-dev 5.2.1+dfsg-1ubuntu14.3 trusty  \
    libcurl4-openssl-dev 7.47.0-1ubuntu2.3 xenial    \
    libicu52 52.1-3ubuntu0.6 trusty

# Get a recent version of CMake
wget https://cmake.org/files/v3.9/cmake-3.9.0-Linux-x86_64.sh
echo y | sh cmake-3.9.0-Linux-x86_64.sh --prefix=cmake
export PATH=/citra/cmake/cmake-3.9.0-Linux-x86_64/bin:$PATH

mkdir build && cd build
cmake .. -DUSE_SYSTEM_CURL=ON -DCMAKE_BUILD_TYPE=Release -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"}
make -j4

ctest -VV -C Release
