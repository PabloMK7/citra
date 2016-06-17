#!/bin/sh

set -e
set -x

if grep -nr '\s$' src *.yml *.txt *.md Doxyfile .gitignore .gitmodules .travis* dist/*.desktop \
                 dist/*.svg dist/*.xml; then
    echo Trailing whitespace found, aborting
    exit 1
fi

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    export CC=gcc-6
    export CXX=g++-6
    export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH

    mkdir build && cd build
    cmake ..
    make -j4

    ctest -VV -C Release
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    set -o pipefail

    export Qt5_DIR=$(brew --prefix)/opt/qt5

    mkdir build && cd build
    cmake .. -GXcode
    xcodebuild -configuration Release | xcpretty -c

    ctest -VV -C Release
fi
