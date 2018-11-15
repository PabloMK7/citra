#!/bin/bash -ex

CITRA_SRC_DIR="/citra"
REPO_DIR="$CITRA_SRC_DIR/repo"

# When the script finishes, unmount the repository and delete sensitive files,
# regardless of whether the build passes or fails
umount "$REPO_DIR"
rm -rf "$REPO_DIR" "/tmp/*"
