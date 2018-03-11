#!/bin/bash -ex

. .travis/common/pre-upload.sh

REV_NAME="citra-linux-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

mkdir "$REV_NAME"

cp build/src/citra/citra "$REV_NAME"
cp build/src/dedicated_room/citra-room "$REV_NAME"
cp build/src/citra_qt/citra-qt "$REV_NAME"

# We need icons on Linux for .desktop entries
mkdir "$REV_NAME/dist"
cp dist/icon.png "$REV_NAME/dist/citra.png"

. .travis/common/post-upload.sh
