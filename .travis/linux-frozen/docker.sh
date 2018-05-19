#!/bin/bash -ex

cd /citra

apt-get update
apt-get install -y build-essential wget git python-launchpadlib ccache

# Install specific versions of packages with their dependencies
# The apt repositories remove older versions regularly, so we can't use
# apt-get and have to pull the packages directly from the archives.
# qt5-qmltooling-plugins and qtdeclarative5-dev are required for qtmultimedia5-dev
/citra/.travis/linux-frozen/install_package.py       \
    libsdl2-dev 2.0.7+dfsg1-3ubuntu1 bionic          \
    qtbase5-dev 5.9.3+dfsg-0ubuntu2 bionic           \
    libqt5opengl5-dev 5.9.3+dfsg-0ubuntu2 bionic     \
    qt5-qmltooling-plugins 5.9.3-0ubuntu1 bionic     \
    qtdeclarative5-dev 5.9.3-0ubuntu1 bionic         \
    qtmultimedia5-dev 5.9.3-0ubuntu3 bionic          \
    libicu57 57.1-6ubuntu0.2 bionic                  \
    cmake 3.10.2-1ubuntu2 bionic

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON
make -j4

ctest -VV -C Release
