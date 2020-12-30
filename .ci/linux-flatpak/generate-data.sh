#!/bin/bash -ex
# This script generates the appdata.xml and org.citra.$REPO_NAME.json files
# needed to define application metadata and build citra depending on what version
# of citra we're building (nightly or canary)

# Converts "citra-emu/citra-nightly" to "citra-nightly"
REPO_NAME=$(echo $TRAVIS_REPO_SLUG | cut -d'/' -f 2)
# Converts "citra-nightly" to "Citra Nightly"
REPO_NAME_FRIENDLY=$(echo $REPO_NAME | sed -e 's/-/ /g' -e 's/\b\(.\)/\u\1/g')

# Generate the correct appdata.xml for the version of Citra we're building
cat > /tmp/appdata.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<application>
  <id type="desktop">org.citra.$REPO_NAME.desktop</id>
  <name>$REPO_NAME_FRIENDLY</name>
  <summary>Nintendo 3DS emulator</summary>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>GPL-2.0</project_license>
  <description>
    <p>Citra is an experimental open-source Nintendo 3DS emulator/debugger written in C++. It is written with portability in mind, with builds actively maintained for Windows, Linux and macOS.</p>
    <p>Citra emulates a subset of 3DS hardware and therefore is useful for running/debugging homebrew applications, and it is also able to run many commercial games! Some of these do not run at a playable state, but we are working every day to advance the project forward. (Playable here means compatibility of at least "Okay" on our game compatibility list.)</p>
  </description>
  <url type="homepage">https://citra-emu.org/</url>
  <url type="donation">https://citra-emu.org/donate/</url>
  <url type="bugtracker">https://github.com/citra-emu/citra/issues</url>
  <url type="faq">https://citra-emu.org/wiki/faq/</url>
  <url type="help">https://citra-emu.org/wiki/home/</url>
  <screenshot>https://raw.githubusercontent.com/citra-emu/citra-web/master/site/static/images/screenshots/01-Super%20Mario%203D%20Land.jpg</screenshot>
  <screenshot>https://raw.githubusercontent.com/citra-emu/citra-web/master/site/static/images/screenshots/02-Mario%20Kart%207.jpg</screenshot>
  <screenshot>https://raw.githubusercontent.com/citra-emu/citra-web/master/site/static/images/screenshots/28-The%20Legend%20of%20Zelda%20Ocarina%20of%20Time%203D.jpg</screenshot>
  <screenshot>https://raw.githubusercontent.com/citra-emu/citra-web/master/site/static/images/screenshots/35-Pok%C3%A9mon%20ORAS.png</screenshot>
  <categories>
    <category>Games</category>
    <category>Emulator</category>
  </categories>
</application>
EOF

# Generate the citra flatpak manifest, appending certain variables depending on
# whether we're building nightly or canary.
cat > /tmp/org.citra.$REPO_NAME.json <<EOF
{
    "app-id": "org.citra.$REPO_NAME",
    "runtime": "org.kde.Platform",
    "runtime-version": "5.13",
    "sdk": "org.kde.Sdk",
    "command": "citra-qt",
    "rename-desktop-file": "citra.desktop",
    "rename-icon": "citra",
    "rename-appdata-file": "org.citra.$REPO_NAME.appdata.xml",
    "build-options": {
        "build-args": [
            "--share=network"
        ],
        "env": {
            "CI": "$CI",
            "TRAVIS": "$TRAVIS",
            "CONTINUOUS_INTEGRATION": "$CONTINUOUS_INTEGRATION",
            "TRAVIS_BRANCH": "$TRAVIS_BRANCH",
            "TRAVIS_BUILD_ID": "$TRAVIS_BUILD_ID",
            "TRAVIS_BUILD_NUMBER": "$TRAVIS_BUILD_NUMBER",
            "TRAVIS_COMMIT": "$TRAVIS_COMMIT",
            "TRAVIS_JOB_ID": "$TRAVIS_JOB_ID",
            "TRAVIS_JOB_NUMBER": "$TRAVIS_JOB_NUMBER",
            "TRAVIS_REPO_SLUG": "$TRAVIS_REPO_SLUG",
            "TRAVIS_TAG": "$TRAVIS_TAG"
        }
    },
    "finish-args": [
        "--device=all",
        "--socket=x11",
        "--socket=pulseaudio",
        "--share=network",
        "--share=ipc",
        "--filesystem=xdg-config/citra-emu:create",
        "--filesystem=xdg-data/citra-emu:create",
        "--filesystem=host:ro"
    ],
    "modules": [
        {
            "name": "citra",
            "buildsystem": "cmake-ninja",
            "builddir": true,
            "config-opts": [
                "-DCMAKE_BUILD_TYPE=Release",
                "-DENABLE_QT_TRANSLATION=ON",
                "-DCITRA_ENABLE_COMPATIBILITY_REPORTING=ON",
                "-DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON",
                "-DENABLE_FFMPEG_VIDEO_DUMPER=ON",
                "-DENABLE_FDK=ON"
            ],
            "cleanup": [
              "/bin/citra",
              "/share/man",
              "/share/pixmaps"
            ],
            "post-install": [
                "install -Dm644 ../appdata.xml /app/share/appdata/org.citra.$REPO_NAME.appdata.xml",
                "desktop-file-install --dir=/app/share/applications ../dist/citra.desktop",
                "sed -i 's/Name=Citra/Name=$REPO_NAME_FRIENDLY/g' /app/share/applications/citra.desktop",
                "echo 'StartupWMClass=citra-qt' >> /app/share/applications/citra.desktop",
                "install -Dm644 ../dist/citra.svg /app/share/icons/hicolor/scalable/apps/citra.svg",
                "install -Dm644 ../dist/icon.png /app/share/icons/hicolor/512x512/apps/citra.png",
                "mv /app/share/mime/packages/citra.xml /app/share/mime/packages/org.citra.$REPO_NAME.xml",
                "sed 's/citra/org.citra.citra-nightly/g' -i /app/share/mime/packages/org.citra.$REPO_NAME.xml"
            ],
            "sources": [
                {
                    "type": "git",
                    "url": "https://github.com/citra-emu/$REPO_NAME.git",
                    "branch": "$TRAVIS_BRANCH",
                    "disable-shallow-clone": true
                },
                {
                    "type": "file",
                    "path": "/tmp/appdata.xml"
                }
            ]
        }
    ]
}
EOF

# Call the script to build citra
/bin/bash -ex /citra/.travis/linux-flatpak/docker.sh
