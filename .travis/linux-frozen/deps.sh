#!/bin/sh -ex

sudo apt-get -y install binutils-gold

docker pull citraemu/build-environments:linux-frozen
