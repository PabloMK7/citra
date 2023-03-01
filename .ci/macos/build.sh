#!/bin/bash -ex

set -o pipefail

export PATH="/usr/local/opt/ccache/libexec:$PATH"
# ccache configurations
export CCACHE_CPP2=yes
export CCACHE_SLOPPINESS=time_macros

export CC="ccache clang"
export CXX="ccache clang++"
export OBJC="clang"
export ASM="clang"

ccache -s

mkdir build && cd build
# TODO: LibreSSL ASM disabled due to platform detection issues in build.
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$TARGET_ARCH" \
    -DENABLE_QT_TRANSLATION=ON \
    -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} \
    -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON \
    -DUSE_DISCORD_PRESENCE=ON \
    -DENABLE_FFMPEG_AUDIO_DECODER=ON \
    -DENABLE_FFMPEG_VIDEO_DUMPER=ON \
    -DENABLE_ASM=OFF \
    -GNinja
ninja

ccache -s

CURRENT_ARCH=`arch`
if [ "$TARGET_ARCH" = "$CURRENT_ARCH" ]; then
  ctest -VV -C Release
fi
