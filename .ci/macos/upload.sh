#!/bin/bash -ex

. .ci/common/pre-upload.sh

REV_NAME="citra-osx-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"

cp -a build/bin/Release/citra "$REV_NAME"
cp -a build/bin/Release/libs "$REV_NAME"
cp -a build/bin/Release/citra-qt.app "$REV_NAME"
cp -a build/bin/Release/citra-room "$REV_NAME"

. .ci/common/post-upload.sh
