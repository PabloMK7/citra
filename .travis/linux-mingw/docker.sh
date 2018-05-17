#!/bin/bash -ex

cd /citra
MINGW_PACKAGES="sdl2-mingw-w64 qt5base-mingw-w64 qt5tools-mingw-w64 libsamplerate-mingw-w64 qt5multimedia-mingw-w64"
apt-get update
apt-get install -y gpg wget git python3-pip ccache g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-tools cmake
# HACK: the repository does not contain necessary packages for 18.04, we'll use packages for 17.10 for now
echo 'deb http://ppa.launchpad.net/tobydox/mingw-w64/ubuntu artful main ' > /etc/apt/sources.list.d/extras.list
apt-key adv --keyserver keyserver.ubuntu.com --recv '72931B477E22FEFD47F8DECE02FE5F12ADDE29B2'
apt-get update
apt-get install -y ${MINGW_PACKAGES}

# fix a problem in current MinGW headers
wget -q https://github.com/Alexpux/mingw-w64/raw/d0d7f784833bbb0b2d279310ddc6afb52fe47a46/mingw-w64-headers/crt/errno.h -O /usr/x86_64-w64-mingw32/include/errno.h
# override Travis CI unreasonable ccache size
echo 'max_size = 3.0G' > "$HOME/.ccache/ccache.conf"

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$(pwd)/../CMakeModules/MinGWCross.cmake" -DUSE_CCACHE=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_TRANSLATION=ON -DCITRA_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON
make -j4

echo "Tests skipped"
#ctest -VV -C Release

ccache -s

echo 'Prepare binaries...'
cd ..
mkdir package

QT_PLATFORM_DLL_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/'
find build/ -name "citra*.exe" -exec cp {} 'package' \;

mkdir package/platforms
cp "${QT_PLATFORM_DLL_PATH}/qwindows.dll" package/platforms/
cp -r "${QT_PLATFORM_DLL_PATH}/../mediaservice/" package/


for i in package/*.exe; do
  # we need to process pdb here, however, cv2pdb
  # does not work here, so we just simply strip all the debug symbols
  x86_64-w64-mingw32-strip "${i}"
done

pip3 install pefile
python3 .travis/linux-mingw/scan_dll.py package/*.exe "package/"
