# PrtSc

PrtSc is a small Windows screenshot utility that runs from the system tray (default hotkey `Ctrl+PrtSc`).

![PrtSc screenshot](assets/app_screenshot.png)

## Requirements

PrtSc supports Windows 7 and newer.

OCR is optional and is available on Windows 10 and newer. Keep `PrtScOcr.dll` next to `PrtSc.exe` to enable it.

## Shortcuts

**Global Hotkeys:**
- <kbd>Ctrl</kbd> + <kbd>PrtSc</kbd> — Start a screenshot capture

**Capture Hotkeys:**
- <kbd>Ctrl</kbd> + <kbd>C</kbd> — Copy the selected area to the clipboard
- <kbd>Ctrl</kbd> + <kbd>S</kbd> — Save the selected area as a PNG file
- <kbd>Ctrl</kbd> + <kbd>O</kbd> — Recognize text in the selected area and copy it to the clipboard, on Windows 10+
- <kbd>Esc</kbd> — Cancel the current capture

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable is created at:

```text
build\Release\PrtSc.exe
```

For a 32-bit build:

```powershell
cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

The OCR DLL is built next to the executable:

```text
build\Release\PrtScOcr.dll
build-x86\Release\PrtScOcr.dll
```

## License

MIT License. See [LICENSE](LICENSE).
