# PrtSc

PrtSc is a small Windows screenshot utility that runs from the system tray (default hotkey `Ctrl+PrtSc`).

![PrtSc screenshot](assets/app_screenshot.png)

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable is created at:

```text
build\Release\PrtSc.exe
```

## License

MIT License. See [LICENSE](LICENSE).
