#!/bin/bash -ex

. .ci/common/pre-upload.sh

REV_NAME="citra-osx-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

ARTIFACTS_LIST=($ARTIFACTS)

# Set up the base artifact to combine into.
BASE_ARTIFACT=${ARTIFACTS_LIST[0]}
BASE_ARTIFACT_ARCH="${BASE_ARTIFACT##*-}"
tar xf $BASE_ARTIFACT/$REV_NAME.tar.gz -C $BASE_ARTIFACT
mv $BASE_ARTIFACT/$REV_NAME $REV_NAME

# Executable binary paths that need to be combined.
BIN_PATHS=(citra citra-room citra-qt.app/Contents/MacOS/citra-qt)

# Dylib paths that need to be combined.
IFS=$'\n'
DYLIB_PATHS=($(cd $REV_NAME && find . -name '*.dylib'))
unset IFS

# Combine all of the executable binaries and dylibs.
for OTHER_ARTIFACT in "${ARTIFACTS_LIST[@]:1}"; do
    OTHER_ARTIFACT_ARCH="${OTHER_ARTIFACT##*-}"

    tar xf $OTHER_ARTIFACT/$REV_NAME.tar.gz -C $OTHER_ARTIFACT

    for BIN_PATH in "${BIN_PATHS[@]}"; do
        lipo -create -output $REV_NAME/$BIN_PATH $REV_NAME/$BIN_PATH $OTHER_ARTIFACT/$REV_NAME/$BIN_PATH
    done

    for DYLIB_PATH in "${DYLIB_PATHS[@]}"; do
        # Only merge if the libraries do not have conflicting arches, otherwise it will fail.
        DYLIB_INFO=`file $REV_NAME/$DYLIB_PATH`
        OTHER_DYLIB_INFO=`file $OTHER_ARTIFACT/$REV_NAME/$DYLIB_PATH`
        if ! [[ "$DYLIB_INFO" =~ "$OTHER_ARTIFACT_ARCH" ]] && ! [[ "$OTHER_DYLIB_INFO" =~ "$BASE_ARTIFACT_ARCH" ]]; then
            lipo -create -output $REV_NAME/$DYLIB_PATH $REV_NAME/$DYLIB_PATH $OTHER_ARTIFACT/$REV_NAME/$DYLIB_PATH
        fi
    done
done

. .ci/common/post-upload.sh
