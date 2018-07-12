#!/bin/bash -ex
mkdir "$HOME/.ccache" || true
docker run --env-file .travis/common/travis-ci.env -e ENABLE_COMPATIBILITY_REPORTING -v $(pwd):/citra -v "$HOME/.ccache":/root/.ccache ubuntu:18.04 /bin/bash -ex /citra/.travis/linux-mingw/docker.sh
