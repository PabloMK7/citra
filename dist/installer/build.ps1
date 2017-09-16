if (!(Test-Path redist\installerbase_win.exe)) {
	echo "Downloading dependencies..."
	if (!(Test-Path redist)) {
		New-Item -path . -name redist -itemtype directory
	}
	Invoke-WebRequest -Uri "https://github.com/citra-emu/ext-windows-bin/raw/master/qtifw/windows.zip" -OutFile windows.zip
	echo "Extracting..."
	Expand-Archive windows.zip -DestinationPath redist
} else {
	echo "Found pre-downloaded redist."
}

echo "Building Qt Installer to '.\citra-installer-windows.exe'..."
.\redist\binarycreator_win.exe -t .\redist\installerbase_win.exe -n -c .\config\config_windows.xml -p .\packages\ citra-installer-windows
