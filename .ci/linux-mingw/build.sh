#!/bin/bash -ex
mkdir "$HOME/.ccache" || true
docker run -v $(pwd):/citra -v "$HOME/.ccache":/root/.ccache citraemu/build-environments:linux-mingw /bin/bash -ex /citra/.ci/linux-mingw/docker.sh
