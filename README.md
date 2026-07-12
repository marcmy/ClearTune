# ClearTune

**A light and dark ClearType tuner for Windows.**

ClearTune is a clean-room, native C++/Win32 ClearType calibration wizard with genuine light and dark sample polarity.

Windows' built-in ClearType tuner presents dark text on a light background. ClearTune keeps the same uncomplicated **choose the clearest sample** flow, but lets the calibration surface use **System**, **Light**, or **Dark** mode. You can tune every active monitor or select a single display.

> **Project status:** early alpha. The portable core and Windows x64 builds are tested in GitHub Actions; real-monitor visual validation is still required before publishing a release.

## What it does

- Native C++20 and Win32; no .NET or third-party runtime.
- Uses the recovered stock five-stage sequence with **2, 6, 3, 6, and 6** choices.
- Three-state **System / Light / Dark** selector.
- `System` follows the Windows app theme. A manual Light or Dark choice is remembered until changed.
- Light mode renders dark text on a near-white surface.
- Dark mode renders light text on the standard near-black Windows app surface.
- Tunes all active monitors or one selected monitor.
- Skips the display-setup page when a landscape monitor is already using an acceptable resolution.
- Shows a warning only for portrait orientation or a known reduced resolution.
- Uses the stock-style DirectWrite bitmap-render-target path and Calibri 11-point samples.
- Previews the selected global ClearType settings as you advance through the wizard.
- Restores the original global settings when you cancel or return to the opening page.
- Persists per-monitor and global settings only when you click **Finish**.
- Captures global and per-display ClearType values before the session.
- Rolls the complete snapshot back if applying any value fails.
- Requires no administrator rights, service, tray process, startup entry, telemetry, or updater.

The selected profile is the monitor's normal Windows ClearType configuration. Theme mode changes the calibration polarity only; it does **not** install a background watcher or swap profiles when Windows changes theme.

## Safety

The app reads and writes the same user-scoped settings used by Windows font rendering:

```text
HKCU\Software\Microsoft\Avalon.Graphics\DISPLAY…
```

During calibration, ClearTune temporarily previews the compatible global font-smoothing values through `SystemParametersInfoW`, using non-persistent calls. Cancel and Back-to-start restore the launch snapshot. Per-monitor registry values and persistent global settings are written only when **Finish** is clicked. A failed final apply automatically restores the launch snapshot.

For an independent before/after record, run:

```powershell
pwsh -File .\tools\Snapshot-ClearType.ps1
```

The script writes a timestamped JSON snapshot in the current directory.

## Build

### Visual Studio 2022

Requirements:

- Windows 10 or Windows 11
- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK
- CMake 3.24 or newer

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is produced at:

```text
build\Release\ClearTune.exe
```

### Portable core tests

The non-Windows model, candidate, theme, conversion, and wizard logic can also be tested with Ninja:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current compatibility status

Static analysis of the supplied Windows 11 ClearType tuner established the stock page order, candidate tables, DirectWrite rendering path, live-preview behavior, compatible registry values, and global SPI operations. ClearTune reproduces those observable behaviors through public Windows interfaces while adding theme polarity and a refreshed native UI. See [`docs/reverse-engineering.md`](docs/reverse-engineering.md).

## Clean-room boundary

This repository contains no Microsoft executables, resources, artwork, or source code. The implementation uses public Win32, DirectWrite, registry, display-configuration, and system-parameter interfaces. The supplied Windows binaries were inspected only to understand observable compatibility behavior and data formats.

## License

MIT. See [LICENSE](LICENSE).
