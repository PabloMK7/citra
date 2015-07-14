if [ "$TRAVIS_BRANCH" = "master" ]; then
    GITDATE="`git show -s --date=short --format='%ad' | sed 's/-//g'`"
    GITREV="`git show -s --format='%h'`"

    if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then
        REV_NAME="citra-${GITDATE}-${GITREV}-linux-amd64"
        UPLOAD_DIR="/citra/nightly/linux-amd64"
        mkdir "$REV_NAME"

        cp build/src/citra/citra "$REV_NAME"
        cp build/src/citra_qt/citra-qt "$REV_NAME"
    elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
        REV_NAME="citra-${GITDATE}-${GITREV}-osx-amd64"
        UPLOAD_DIR="/citra/nightly/osx-amd64"
        mkdir "$REV_NAME"

        brew install lftp
        cp build/src/citra/Release/citra "$REV_NAME"
        cp -r build/src/citra_qt/Release/citra-qt.app "$REV_NAME"

        # move qt libs into app bundle for deployment
        $(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/citra-qt.app"
    fi

    ARCHIVE_NAME="${REV_NAME}.tar.xz"
    tar -cJvf "$ARCHIVE_NAME" "$REV_NAME"
    lftp -c "open -u citra-builds,$BUILD_PASSWORD sftp://builds.citra-emu.org; put -O '$UPLOAD_DIR' '$ARCHIVE_NAME'"
fi
