#!/bin/bash -ex
mkdir "$HOME/.ccache" || true
docker run --env-file .travis/common/travis-ci.env -v $(pwd):/citra -v "$HOME/.ccache":/root/.ccache citraemu/build-environments:linux-mingw /bin/bash -ex /citra/.travis/linux-mingw/docker.sh
