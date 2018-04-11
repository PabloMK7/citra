#!/bin/bash -ex
mkdir "$HOME/.ccache" || true
docker pull ubuntu:18.04
docker run -e ENABLE_COMPATIBILITY_REPORTING -v $(pwd):/citra -v "$HOME/.ccache":/root/.ccache ubuntu:18.04  /bin/bash -ex /citra/.travis/linux-frozen/docker.sh
