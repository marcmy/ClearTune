# Testing guide

## Automated tests

### Portable core

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The core suite covers theme resolution, persisted theme values, display-key normalization, CRU-style preferred-resolution handling, the recovered **2/6/3/6/6** candidate model, rendering/SPI conversions, and multi-monitor wizard navigation.

### Windows build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The Windows application target compiling cleanly is also a header/API smoke test for the Win32, DirectWrite 1, GDI interop, and settings-preview layers.

## Manual release checklist

1. Export a baseline with `tools/Snapshot-ClearType.ps1`.
2. Launch on a Windows 10 or Windows 11 x64 machine without elevation.
3. Confirm System follows the current Windows app theme.
4. Select Light, restart, and confirm Light persists.
5. Select Dark, restart, and confirm Dark persists.
6. Return to System and confirm it follows Windows again.
7. Confirm the five pages contain 2, 6, 3, 6, and 6 cards, in that order.
8. On every calibration page, switch theme and confirm the current candidate selection remains unchanged.
9. Advance through pages while watching another ClearType-rendered window; confirm compatible global settings preview as pages advance.
10. Press Back to the opening page and confirm the launch-time global settings are restored.
11. Cancel from several different pages and compare the baseline snapshot: the final state must match the launch snapshot.
12. With two monitors, confirm the wizard moves to the monitor named on the page.
13. Verify both all-monitor and single-monitor calibration.
14. Verify 100%, 125%, 150%, and 200% DPI layouts where available.
15. Verify landscape and portrait monitors.
16. Finish a session and confirm the selected per-display values are written.
17. Disconnect a secondary monitor before Finish and confirm it is skipped with a warning.
18. Force a registry write failure in a disposable test account and verify the baseline is restored.
19. Run the stock ClearType tuner afterward and confirm it can read and supersede the settings normally.

Do not publish a release solely from a Linux portable-core build. A Windows MSVC build and manual display test are required.
