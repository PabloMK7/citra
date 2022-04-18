#!/bin/bash -ex

. ./.ci/common/pre-upload.sh

REV_NAME="citra-${GITDATE}-${GITREV}"

cp src/android/app/build/outputs/apk/release/app-release.apk \
  "artifacts/${REV_NAME}.apk"
cp src/android/app/build/outputs/bundle/release/app-release.aab \
  "artifacts/${REV_NAME}.aab"
