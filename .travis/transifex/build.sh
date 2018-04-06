#!/bin/bash -e

echo "+docker run -e TRANSIFEX_API_TOKEN=[REDACTED] -v $(pwd):/citra alpine /bin/sh -e /citra/.travis/transifex/docker.sh"
docker run -e TRANSIFEX_API_TOKEN="${TRANSIFEX_API_TOKEN}" -v "$(pwd)":/citra alpine /bin/sh -e /citra/.travis/transifex/docker.sh
