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
dylibbundler -b -x "${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt" -cd -d "${REV_NAME}/citra-qt.app/Contents/Frameworks/" -p "@executable_path/../Frameworks/" -of
# move qt frameworks into app bundle for deployment
$(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/citra-qt.app" -executable="${REV_NAME}/citra-qt.app/Contents/MacOS/citra-qt"

# move libs into folder for deployment
dylibbundler -b -x "${REV_NAME}/citra" -cd -d "${REV_NAME}/libs" -p "@executable_path/libs/"

# TODO(merry): Figure out why these libraries are not automatically processed
install_name_tool -change /usr/local/Cellar/ffmpeg/4.1_1/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib "${REV_NAME}/citra-qt.app/Contents/Frameworks/libavcodec.58.dylib"
install_name_tool -change /usr/local/Cellar/ffmpeg/4.1_1/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib "${REV_NAME}/citra-qt.app/Contents/Frameworks/libswresample.3.dylib"
install_name_tool -change /usr/local/Cellar/ffmpeg/4.1_1/lib/libavutil.56.dylib @executable_path/libs/libavutil.56.dylib "${REV_NAME}/libs/libavcodec.58.dylib"
install_name_tool -change /usr/local/Cellar/ffmpeg/4.1_1/lib/libavutil.56.dylib @executable_path/libs/libavutil.56.dylib "${REV_NAME}/libs/libswresample.3.dylib"
install_name_tool -change /usr/local/Cellar/libvorbis/1.3.6/lib/libvorbis.0.dylib @executable_path/libs/libavutil.56.dylib "${REV_NAME}/libs/libvorbisenc.2.dylib"

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
