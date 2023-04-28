#!/bin/bash -ex

. .ci/common/pre-upload.sh
REV_NAME="citra-linux-${GITDATE}-${GITREV}"
mv build/Citra*.AppImage "${GITHUB_WORKSPACE}"/artifacts/${REV_NAME}.AppImage
