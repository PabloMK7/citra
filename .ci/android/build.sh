#!/bin/bash -ex

export NDK_CCACHE=$(which ccache)

ccache -s

cd src/android
chmod +x ./gradlew
./gradlew bundleRelease
./gradlew assembleRelease

ccache -s
