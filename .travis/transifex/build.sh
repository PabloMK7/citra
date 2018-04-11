#!/bin/bash -e

docker run -e TRANSIFEX_API_TOKEN="${TRANSIFEX_API_TOKEN}" -v "$(pwd)":/citra alpine /bin/sh -e /citra/.travis/transifex/docker.sh
