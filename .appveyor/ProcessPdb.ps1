# Generate pdb files for mingw
if ($env:BUILD_TYPE -eq 'mingw') {
  Invoke-WebRequest -Uri https://raw.githubusercontent.com/citra-emu/ext-windows-bin/master/cv2pdb/cv2pdb.exe -OutFile cv2pdb.exe
  foreach ($exe in Get-ChildItem "$RELEASE_DIST" -Recurse -Filter "citra*.exe") {
    .\cv2pdb $exe.FullName
  }
}

# Specify source locations in pdb via srcsrv.ini
$srcsrv = "SRCSRV: ini ------------------------------------------------`r`n"
$srcsrv += "VERSION=2`r`n"
$srcsrv += "VERCTRL=http`r`n"
$srcsrv += "SRCSRV: variables ------------------------------------------`r`n"
$srcsrv += "SRCSRVTRG=https://raw.githubusercontent.com/%var2%/%var3%/%var4%`r`n"
$srcsrv += "SRCSRV: source files ---------------------------------------`r`n"
foreach ($repo in @{
  "citra-emu/citra" = ""
  "citra-emu/ext-boost" = "externals/boost"
  "citra-emu/ext-soundtouch" = "externals/soundtouch"
  "fmtlib/fmt" = "externals/fmt"
  "herumi/xbyak" = "externals/xbyak"
  "lsalzman/enet" = "externals/enet"
  "MerryMage/dynarmic" = "externals/dynarmic"
  "neobrain/nihstro" = "externals/nihstro"
}.GetEnumerator()) {
  pushd
  cd $repo.Value
  $rev = git rev-parse HEAD
  $files = git ls-tree --name-only --full-tree -r HEAD
  foreach ($file in $files) {
    $srcsrv += "$(pwd)\$($file -replace '/','\')*$($repo.Name)*$rev*$file`r`n"
  }
  popd
}
$srcsrv += "SRCSRV: end ------------------------------------------------`r`n"
Set-Content -Path srcsrv.ini -Value $srcsrv
foreach ($pdb in Get-ChildItem "$RELEASE_DIST" -Recurse -Filter "*.pdb") {
  & "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\srcsrv\pdbstr.exe" -w -i:srcsrv.ini -p:$pdb.FullName -s:srcsrv
}
