#!/bin/bash -ex

mkdir -p ~/bin/gold
echo '#!/bin/bash' > ~/bin/gold/ld
echo 'gold "$@"' >> ~/bin/gold/ld
chmod a+x ~/bin/gold/ld
export CFLAGS="-B$HOME/bin/gold $CFLAGS"
export CXXFLAGS="-B$HOME/bin/gold $CXXFLAGS"

cd /citra

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++
ninja

ctest -VV -C Release
