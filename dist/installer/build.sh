#!/bin/bash

set -e

PLATFORM=""
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="mac"
else
    echo Your platform is not supported.
    exit 1
fi
        
if [ ! -f redist/installerbase_$PLATFORM ]; then
    echo Downloading dependencies...
    curl -L -O https://github.com/citra-emu/ext-windows-bin/raw/master/qtifw/$PLATFORM.tar.gz

    echo Extracting...
    mkdir -p redist
    cd redist
    tar -zxvf ../$PLATFORM.tar.gz
    cd ..

    chmod +x redist/*
fi

TARGET_FILE=citra-installer-$PLATFORM
CONFIG_FILE=config/config_$PLATFORM.xml
REDIST_BASE=redist/installerbase_$PLATFORM
BINARY_CREATOR=redist/binarycreator_$PLATFORM
PACKAGES_DIR=packages

echo Building to \'$TARGET_FILE\'...

$BINARY_CREATOR -t $REDIST_BASE -n -c $CONFIG_FILE -p $PACKAGES_DIR $TARGET_FILE
