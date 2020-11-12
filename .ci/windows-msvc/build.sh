#!/bin/sh -ex

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/../CMakeModules/MSVCCache.cmake" -DUSE_CCACHE=ON -DCITRA_USE_BUNDLED_QT=1 -DCITRA_USE_BUNDLED_SDL2=1 -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${COMPAT} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_MF=ON -DENABLE_FFMPEG_VIDEO_DUMPER=ON

ninja
# show the caching efficiency
buildcache -s

ctest -VV -C Release || echo "::error ::Test error occurred on Windows MSVC build"
