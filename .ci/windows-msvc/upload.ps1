
$GITDATE = $(git show -s --date=short --format='%ad') -replace "-", ""
$GITREV = $(git show -s --format='%h')

# Find out what release we are building
if ( $env:GITHUB_REF_NAME -like "*canary-*" -or $env:GITHUB_REF_NAME -like "*nightly-*" ) {
    $RELEASE_NAME = ${env:GITHUB_REF_NAME}.split("-")[0]
    $RELEASE_NAME = "${RELEASE_NAME}-msvc"
}
else {
    $RELEASE_NAME = "head"
}

$MSVC_BUILD_ZIP = "citra-windows-msvc-$GITDATE-$GITREV.zip" -replace " ", ""
$MSVC_SEVENZIP = "citra-windows-msvc-$GITDATE-$GITREV.7z" -replace " ", ""

$BUILD_DIR = ".\build\bin\Release"

# Create artifact directories
mkdir $RELEASE_NAME
mkdir "artifacts"

echo "Starting to pack ${RELEASE_NAME}"

Copy-Item $BUILD_DIR\* -Destination $RELEASE_NAME -Recurse
Remove-Item $RELEASE_NAME\tests.* -ErrorAction ignore
Remove-Item $RELEASE_NAME\*.pdb -ErrorAction ignore

# Copy documentation
Copy-Item license.txt -Destination $RELEASE_NAME
Copy-Item README.md -Destination $RELEASE_NAME

# Copy cross-platform scripting support
Copy-Item dist\scripting -Destination $RELEASE_NAME -Recurse

# Build the final release artifacts
7z a -tzip $MSVC_BUILD_ZIP $RELEASE_NAME\*
7z a $MSVC_SEVENZIP $RELEASE_NAME

Copy-Item $MSVC_BUILD_ZIP -Destination "artifacts"
Copy-Item $MSVC_SEVENZIP -Destination "artifacts"
