#!/bin/bash -e

# Setup RC file for tx
echo $'[https://www.transifex.com]\nhostname = https://www.transifex.com\nusername = api\npassword = '"$TRANSIFEX_API_TOKEN"$'\n' > ~/.transifexrc

set -x

echo -e "\e[1m\e[33mInstalling dependencies...\e[0m"
apk update
apk add build-base sdl2-dev cmake python3-dev qt5-qttools-dev

pip3 install transifex-client

echo -e "\e[1m\e[33mBuild tools information:\e[0m"
cmake --version
gcc -v
tx --version

cd /citra
mkdir build && cd build
cmake .. -DENABLE_QT_TRANSLATION=ON -DGENERATE_QT_TRANSLATION=ON -DCMAKE_BUILD_TYPE=Release
make translation
cd ..

cd dist/languages
tx push -s
