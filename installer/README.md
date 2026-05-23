# PrtSc MSI installer

This folder contains the WiX v7 MSI packaging files for PrtSc.

Prerequisites:

```powershell
wix extension add WixToolset.UI.wixext/7.0.0
```

Build the per-user x64 MSI:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build-msi.ps1
```

The script:

- stops any running `PrtSc` process so the linker can overwrite build outputs;
- configures and builds `build-msi-x64` with the `x64` platform;
- creates `out\msi\PrtSc-<version>-x64.msi` with WiX Toolset v7.

The MSI installs for the current user by default:

- application files: `%LOCALAPPDATA%\Programs\PrtSc`;
- Start Menu shortcut: current user's Start Menu;
- optional startup entry: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

Silent install options:

```powershell
msiexec.exe /i .\out\msi\PrtSc-0.5.0-x64.msi /qn LAUNCH_PRTSC=1 PRTSC_STARTUP=1
```

Pass a different WiX location if needed:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build-msi.ps1 -WixBin 'C:\Path\To\WiX\bin'
```
