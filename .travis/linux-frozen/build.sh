#!/bin/bash -ex

docker pull ubuntu:16.04
docker run -v $(pwd):/citra ubuntu:16.04 /bin/bash -ex /citra/.travis/linux-frozen/docker.sh
