#!/bin/bash -ex

. .travis/common/pre-upload.sh

REV_NAME="citra-osx-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"

cp build/bin/citra "$REV_NAME"
cp -r build/bin/citra-qt.app "$REV_NAME"
cp build/bin/citra-room "$REV_NAME"

# move libs into folder for deployment
macpack "${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt" -d "../Frameworks"
# move qt frameworks into app bundle for deployment
$(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/citra-qt.app" -executable="${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt"

# move libs into folder for deployment
macpack "${REV_NAME}/citra" -d "libs"

# Make the citra-qt.app application launch a debugging terminal.
# Store away the actual binary
mv ${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt ${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt-bin

cat > ${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt <<EOL
#!/usr/bin/env bash
cd "\`dirname "\$0"\`"
chmod +x citra-qt-bin
open citra-qt-bin --args "\$@"
EOL
# Content that will serve as the launching script for citra (within the .app folder)

# Make the launching script executable
chmod +x ${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt

# Verify loader instructions
find "$REV_NAME" -exec otool -L {} \;

. .travis/common/post-upload.sh
