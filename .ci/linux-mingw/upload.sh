#!/bin/bash -ex

. .ci/common/pre-upload.sh

REV_NAME="citra-windows-mingw-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"
# get around the permission issues
cp -r package/* "$REV_NAME"

. .ci/common/post-upload.sh
