#!/bin/bash -ex

set -o pipefail

export MACOSX_DEPLOYMENT_TARGET=10.12
export Qt5_DIR=$(brew --prefix)/opt/qt5
export PATH="/usr/local/opt/ccache/libexec:$PATH"

mkdir build && cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;x86_64h" -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON
make -j4

ctest -VV -C Release
