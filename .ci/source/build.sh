#!/bin/bash -ex

. .ci/common/pre-upload.sh
REV_NAME="citra-unified-source-${GITDATE}-${GITREV}"
COMPAT_LIST='dist/compatibility_list/compatibility_list.json'

pip3 install git-archive-all
wget -q https://api.citra-emu.org/gamedb -O "${COMPAT_LIST}"
git describe --abbrev=0 --always HEAD > GIT-COMMIT
git describe --tags HEAD > GIT-TAG || echo 'unknown' > GIT-TAG
git archive-all --include "${COMPAT_LIST}" --include GIT-COMMIT --include GIT-TAG --force-submodules artifacts/"${REV_NAME}.tar"

cd artifacts/
xz -T0 -9 "${REV_NAME}.tar"
sha256sum "${REV_NAME}.tar.xz" > "${REV_NAME}.tar.xz.sha256sum"
cd ..
