# ClearTune

**A light and dark ClearType tuner for Windows.**

ClearTune is a clean-room, native C++/Win32 ClearType calibration wizard with genuine light and dark sample polarity.

Windows' built-in ClearType tuner presents dark text on a light background. ClearTune keeps the same uncomplicated **choose the clearest sample** flow, but lets the calibration surface use **System**, **Light**, or **Dark** mode. Each active monitor is tuned separately, and the wizard moves onto the display currently being calibrated.

> **Project status:** early alpha. The portable core is tested; the Windows executable is intended for Windows 10/11 x64 and should be validated through the included GitHub Actions build before publishing a release.

## What it does

- Native C++20 and Win32; no .NET or third-party runtime.
- Stock-shaped wizard with five sample pages: 2, 3, 6, 6, and 6 choices.
- Three-state **System / Light / Dark** selector.
- `System` follows the Windows app theme. A manual Light or Dark choice is remembered until changed.
- Light mode renders dark text on a near-white surface.
- Dark mode renders light text on the standard near-black Windows app surface.
- Detects and calibrates all active monitors independently.
- Keeps every choice in memory until **Finish**. Unchecking ClearType skips calibration and preserves the existing per-monitor profiles.
- Captures global and per-display ClearType values before the session.
- Rolls the complete snapshot back if applying any value fails.
- Requires no administrator rights, service, tray process, startup entry, telemetry, or updater.

The selected profile is the monitor's normal Windows ClearType configuration. Theme mode changes the calibration polarity only; it does **not** install a background watcher or swap profiles when Windows changes theme.

## Safety

The app reads and writes the same user-scoped settings used by Windows font rendering:

```text
HKCU\Software\Microsoft\Avalon.Graphics\DISPLAY…
```

It also applies the compatible global font-smoothing values through `SystemParametersInfoW`. Nothing is written before **Finish**. Cancel simply exits. A failed apply automatically restores the launch snapshot.

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

Static analysis of the supplied Windows 11 ClearType tuner established the compatible registry value names, global SPI operations, runtime-generated sample text, and stock page choice counts. The exact private candidate arrays were not copied or recovered. Version 0.1 therefore uses conservative, documented DirectWrite-compatible ranges while preserving the stock-shaped flow. See [`docs/reverse-engineering.md`](docs/reverse-engineering.md).

## Clean-room boundary

This repository contains no Microsoft executables, resources, artwork, or source code. The implementation uses public Win32, Direct2D, DirectWrite, registry, and system-parameter interfaces. The supplied Windows binaries were inspected only to understand observable compatibility behavior and data formats.

## License

MIT. See [LICENSE](LICENSE).
