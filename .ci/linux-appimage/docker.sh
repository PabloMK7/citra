#!/bin/bash

#Building Citra
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_FFMPEG_VIDEO_DUMPER=ON
ninja

ctest -VV -C Release

#Building AppDir
DESTDIR="./AppDir" ninja install
mv ./AppDir/usr/local/bin ./AppDir/usr
mv ./AppDir/usr/local/share ./AppDir/usr
rm -rf ./AppDir/usr/local

#Circumvent missing LibFuse in Docker, by extracting the AppImage
export APPIMAGE_EXTRACT_AND_RUN=1

#Copy External Libraries
mkdir -p ./AppDir/usr/plugins/platformthemes
mkdir -p ./AppDir/usr/plugins/styles
cp /usr/lib/x86_64-linux-gnu/qt5/plugins/platformthemes/libqt5ct.so ./AppDir/usr/plugins/platformthemes
cp /usr/lib/x86_64-linux-gnu/qt5/plugins/platformthemes/libqgtk3.so ./AppDir/usr/plugins/platformthemes
cp /usr/lib/x86_64-linux-gnu/qt5/plugins/styles/libqt5ct-style.so ./AppDir/usr/plugins/styles

#Build AppImage
/linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
