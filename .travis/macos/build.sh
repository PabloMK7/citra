#!/bin/bash -ex

set -o pipefail

export MACOSX_DEPLOYMENT_TARGET=10.13
export Qt5_DIR=$(brew --prefix)/opt/qt5
export PATH="/usr/local/opt/ccache/libexec:/usr/local/opt/llvm/bin:$PATH"

export CC="clang"
export CXX="clang++"
export LDFLAGS="-L/usr/local/opt/llvm/lib"
export CPPFLAGS="-I/usr/local/opt/llvm/include"

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_FFMPEG_AUDIO_DECODER=ON -DENABLE_FFMPEG_VIDEO_DUMPER=ON
# make -j4 takes more than 50 minutes when there is no ccache available on Travis CI
# and when Travis CI timeouts a job after 50 minutes it won't store any ccache get so far.
# To avoid to be stuck forever with failing build, gtimeout will stop make command before
# Travis CI timeouts, and this will allow Travis CI to successfully store any ccache get so far,
# and iterating this process, the ccache will build up till the make command will succeed.
# 50 minutes == 3000 seconds; ~1000 seconds are needed by deps.sh; hence:
gtimeout 1500 make -j4

ctest -VV -C Release
