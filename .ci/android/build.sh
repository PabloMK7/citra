#!/bin/bash -ex

export NDK_CCACHE=$(which ccache)
[ "$GITHUB_REPOSITORY" = "citra-emu/citra-canary" ] &&
   BUILD_FLAVOR=canary ||
   BUILD_FLAVOR=nightly

ccache -s

cd src/android
chmod +x ./gradlew
./gradlew assemble${BUILD_FLAVOR}Release
./gradlew bundle${BUILD_FLAVOR}Release

ccache -s
