#!/bin/bash -ex

GITDATE="`git show -s --date=short --format='%ad' | sed 's/-//g'`"
GITREV="`git show -s --format='%h'`"
REV_NAME="citra-${OS}-${TARGET}-${GITDATE}-${GITREV}"

# Find out what release we are building
if [[ "$GITHUB_REF_NAME" =~ ^canary- ]] || [[ "$GITHUB_REF_NAME" =~ ^nightly- ]]; then
    RELEASE_NAME=$(echo $GITHUB_REF_NAME | cut -d- -f1)
else
    RELEASE_NAME=head
fi

mkdir -p artifacts

if [ -z "${UPLOAD_RAW}" ]; then
    # Archive and upload the artifacts.
    mkdir "$REV_NAME"
    mv build/bundle/* "$REV_NAME"

    if [ "$OS" = "windows" ]; then
        ARCHIVE_NAME="${REV_NAME}.zip"
        powershell Compress-Archive "$REV_NAME" "$ARCHIVE_NAME"
    else
        ARCHIVE_NAME="${REV_NAME}.tar.gz"
        tar czvf "$ARCHIVE_NAME" "$REV_NAME"
    fi

    mv "$REV_NAME" $RELEASE_NAME
    7z a "$REV_NAME.7z" $RELEASE_NAME

    mv "$ARCHIVE_NAME" artifacts/
    mv "$REV_NAME.7z" artifacts/
else
    # Directly upload the raw artifacts, renamed with the revision.
    for ARTIFACT in build/bundle/*; do
        FILENAME=$(basename "$ARTIFACT")
        EXTENSION="${FILENAME##*.}"
        mv "$ARTIFACT" "artifacts/$REV_NAME.$EXTENSION"
    done
fi
