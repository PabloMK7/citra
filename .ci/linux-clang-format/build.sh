#!/bin/bash -ex
mkdir -p "$HOME/.ccache"
docker run -v $(pwd):/citra -v "$HOME/.ccache":/root/.ccache citraemu/build-environments:linux-clang-format /bin/bash -ex /citra/.ci/clang-format/docker.sh
