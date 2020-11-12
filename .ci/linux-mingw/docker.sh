#!/bin/bash -ex

# override CI ccache size
mkdir -p "$HOME/.ccache/"
echo 'max_size = 3.0G' > "$HOME/.ccache/ccache.conf"

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/../CMakeModules/MinGWCross.cmake" -DUSE_CCACHE=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_MF=ON -DENABLE_FFMPEG_VIDEO_DUMPER=ON -DCMAKE_NO_SYSTEM_FROM_IMPORTED=TRUE -DCOMPILE_WITH_DWARF=OFF
ninja

echo "Tests skipped"
#ctest -VV -C Release

ccache -s

echo 'Prepare binaries...'
cd ..
mkdir package

QT_PLATFORM_DLL_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/'
find build/ -name "citra*.exe" -exec cp {} 'package' \;

# copy Qt plugins
mkdir package/platforms
cp "${QT_PLATFORM_DLL_PATH}/qwindows.dll" package/platforms/
cp -rv "${QT_PLATFORM_DLL_PATH}/../mediaservice/" package/
cp -rv "${QT_PLATFORM_DLL_PATH}/../imageformats/" package/
rm -f package/mediaservice/*d.dll

python3 .ci/linux-mingw/scan_dll.py package/*.exe package/imageformats/*.dll "package/"
