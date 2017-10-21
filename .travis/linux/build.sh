#!/bin/bash -ex

docker run -v $(pwd):/citra ubuntu:16.04 /bin/bash -ex /citra/.travis/linux/docker.sh
