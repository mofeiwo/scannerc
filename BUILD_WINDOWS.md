# Windows Build Guide

This project contains a Qt-based Windows cache scanner GUI and a console scanner.

## Files

- `windows_cache_scanner_gui.exe`: Qt Widgets GUI
- `windows_cache_scanner_console.exe`: console scanner
- `windows_cache_rules.json`: scan rules

## Prerequisites on Windows

1. Install Qt for Windows
   - Example: `C:\Qt\6.8.2\msvc2022_64`
2. Install a supported C++ toolchain
   - Recommended: Visual Studio 2022 with C++ workload
3. Install CMake
4. Open a Developer Command Prompt for Visual Studio

## Fast Build

In the `docs` folder, run:

```bat
set QT_ROOT=C:\Qt\6.8.2\msvc2022_64
build_windows_qt.bat
```

If the build succeeds, the output is usually here:

```text
docs\build-win\windows_cache_scanner_gui.exe
```

The script also copies:

```text
docs\build-win\windows_cache_rules.json
```

## Run

Double-click:

```text
windows_cache_scanner_gui.exe
```

Make sure `windows_cache_rules.json` stays in the same folder as the exe, or choose it manually in the UI.

## Manual CMake Build

```bat
set QT_ROOT=C:\Qt\6.8.2\msvc2022_64
cmake -G "Ninja" -DCMAKE_PREFIX_PATH=%QT_ROOT% -S . -B build-win
cmake --build build-win --config Release --target windows_cache_scanner_gui
%QT_ROOT%\bin\windeployqt.exe build-win\windows_cache_scanner_gui.exe
copy /Y windows_cache_rules.json build-win\windows_cache_rules.json
```
