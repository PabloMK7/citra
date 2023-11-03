#!/bin/bash -ex

# TODO: Work around pip install issues with Python 3.12 in the GitHub runner image.
# See: https://github.com/actions/runner-images/issues/8709
PYTHON_PATH=$(brew --prefix python@3.11)
export PATH=$PYTHON_PATH/bin:$PYTHON_PATH/libexec/bin:$PATH

mkdir build && cd build
cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DENABLE_QT_TRANSLATION=ON \
    -DCITRA_ENABLE_COMPATIBILITY_REPORTING=ON \
    -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON
ninja

ccache -s -v
