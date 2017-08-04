#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
    docker pull ubuntu:16.04
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew update
    brew install qt5 sdl2 dylibbundler p7zip
fi
