Citra Qt Installer
==================

This contains the configuration files for building Citra's installer.

`packages` is empty as Qt expects that it gets a valid directory for offline
 packages, even if you are a online-only installer.

Installers can only be built on the platform that they are targeting.

Windows
-------

Using Powershell 2.0 (Windows 10):

```powershell
cd dist\installer
powershell –ExecutionPolicy Bypass .\build.ps1
```

Linux/Mac
---------

Curl + Bash must be available.

```bash
cd dist/installer
chmod +x build.sh
./build.sh
```
