#!/bin/bash

set -e
set -x

if grep -nr '\s$' src *.yml *.txt *.md Doxyfile .gitignore .gitmodules .travis* dist/*.desktop \
                 dist/*.svg dist/*.xml; then
    echo Trailing whitespace found, aborting
    exit 1
fi

for f in $(git diff --name-only --diff-filter=ACMRTUXB --cached); do
    if ! echo "$f" | egrep -q "[.](cpp|h)$"; then
        continue
    fi
    if ! echo "$f" | egrep -q "^src/"; then
        continue
    fi
    d=$(diff -u "$f" <(clang-format "$f"))
    if ! [ -z "$d" ]; then
        echo "!!! $f not compliant to coding style, here is the fix:"
        echo "$d"
        fail=1
    fi
done

if [ "$fail" = 1 ]; then
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

    export MACOSX_DEPLOYMENT_TARGET=10.9
    export Qt5_DIR=$(brew --prefix)/opt/qt5

    mkdir build && cd build
    cmake .. -GXcode
    xcodebuild -configuration Release

    ctest -VV -C Release
fi
