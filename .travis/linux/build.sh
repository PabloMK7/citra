#!/bin/bash -ex

docker run -v $(pwd):/citra ubuntu:16.04 /bin/bash /citra/.travis/linux/docker.sh
