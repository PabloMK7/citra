#!/bin/sh -ex

BUILDCACHE_VERSION="0.22.3"

choco install wget ninja
# Install buildcache
wget "https://github.com/mbitsnbites/buildcache/releases/download/v${BUILDCACHE_VERSION}/buildcache-win-mingw.zip"
7z x 'buildcache-win-mingw.zip'
mv ./buildcache/bin/buildcache.exe "/c/ProgramData/chocolatey/bin"
rm -rf ./buildcache/
