#!/bin/bash -ex

. ./.ci/common/pre-upload.sh

REV_NAME="citra-${GITDATE}-${GITREV}"
[ "${GITHUB_REPOSITORY}" = "citra-emu/citra-canary" ] &&
   BUILD_FLAVOR=canary ||
   BUILD_FLAVOR=nightly

cp src/android/app/build/outputs/apk/${BUILD_FLAVOR}/release/app-${BUILD_FLAVOR}-release.apk \
  "artifacts/${REV_NAME}.apk"
cp src/android/app/build/outputs/bundle/${BUILD_FLAVOR}Release/app-${BUILD_FLAVOR}-release.aab \
  "artifacts/${REV_NAME}.aab"

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]
then
  echo "Signing apk..."
  base64 --decode <<< "${ANDROID_KEYSTORE_B64}" > ks.jks

  apksigner sign --ks ks.jks \
    --ks-key-alias "${ANDROID_KEY_ALIAS}" \
    --ks-pass env:ANDROID_KEYSTORE_PASS "artifacts/${REV_NAME}.apk"
fi
