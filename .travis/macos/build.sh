#!/bin/bash -ex

set -o pipefail

export MACOSX_DEPLOYMENT_TARGET=10.9
export Qt5_DIR=$(brew --prefix)/opt/qt5

mkdir build && cd build
cmake .. -DUSE_SYSTEM_CURL=ON -DCMAKE_OSX_ARCHITECTURES="x86_64;x86_64h" -DCMAKE_BUILD_TYPE=Release
make -j4

ctest -VV -C Release
