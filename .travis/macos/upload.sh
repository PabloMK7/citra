#!/bin/bash -ex

. .travis/common/pre-upload.sh

REV_NAME="citra-osx-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"

cp build/bin/Release/citra "$REV_NAME"
cp -r build/bin/Release/citra-qt.app "$REV_NAME"
cp build/bin/Release/citra-room "$REV_NAME"

# move libs into folder for deployment
macpack "${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt" -d "../Frameworks"
# move qt frameworks into app bundle for deployment
$(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/citra-qt.app" -executable="${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt"

# move libs into folder for deployment
macpack "${REV_NAME}/citra" -d "libs"

# Make the launching script executable
chmod +x ${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt

# Verify loader instructions
find "$REV_NAME" -exec otool -L {} \;

. .travis/common/post-upload.sh
