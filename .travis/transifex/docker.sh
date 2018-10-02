#!/bin/bash -e

# Setup RC file for tx
echo $'[https://www.transifex.com]\nhostname = https://www.transifex.com\nusername = api\npassword = '"$TRANSIFEX_API_TOKEN"$'\n' > ~/.transifexrc

set -x

cat << 'EOF' > /usr/bin/tx
#!/usr/bin/python3

# -*- coding: utf-8 -*-
import re
import sys

from txclib.cmdline import main

if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
EOF

echo -e "\e[1m\e[33mBuild tools information:\e[0m"
cmake --version
gcc -v
tx --version

cd /citra
mkdir build && cd build
cmake .. -DENABLE_QT_TRANSLATION=ON -DGENERATE_QT_TRANSLATION=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_SDL2=OFF -DENABLE_SCRIPTING=OFF
make translation
cd ..

cd dist/languages
tx push -s
