#!/bin/bash -ex

docker pull ubuntu:18.04
docker run -e ENABLE_COMPATIBILITY_REPORTING -v $(pwd):/citra ubuntu:18.04 /bin/bash -ex /citra/.travis/linux-frozen/docker.sh
