# Testing guide

## Automated tests

### Portable core

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The core suite covers theme and comparison-polarity resolution, persisted theme values, display-key normalization, CRU-style preferred-resolution handling, monitor-map scaling and hit testing, recovered global-contrast candidate generation, per-monitor candidate mappings, and multi-monitor wizard navigation.

### Windows build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The Windows application target compiling cleanly is also a header/API smoke test for the Win32, DirectWrite 1, GDI interop, monitor-map, and settings-preview layers.

## Manual release checklist

1. Export a baseline with `tools/Snapshot-ClearType.ps1`.
2. Launch on a Windows 10 or Windows 11 x64 machine without elevation.
3. Confirm System follows the current Windows app theme.
4. Select Light, restart, and confirm Light persists.
5. Select Dark, restart, and confirm Dark persists.
6. Return to System and confirm it follows Windows again.
7. With multiple monitors, confirm the welcome map matches Windows' relative positions, proportions, and portrait/landscape orientation.
8. Hover each monitor and confirm it receives a thin blue outline without changing the radio selection.
9. Click a monitor while Tune All is active and confirm Tune One is selected automatically with a thicker outline around that monitor.
10. Use Left/Right and Up/Down while the map has keyboard focus and confirm selection wraps through the displays.
11. Confirm the map remains legible and correctly scaled in both Light and Dark modes and at 100%, 125%, 150%, and 200% DPI.
12. Confirm the first selected monitor has 2, 6, 3, 6, and 6 cards in order.
13. Confirm each later selected monitor skips the six-card global-contrast page and shows 2, 3, 6, and 6 cards.
14. Tune only a secondary monitor and confirm the global-contrast page still appears once.
15. On every calibration page, change the theme and confirm the current candidate selection remains unchanged.
16. Use Compare Light/Dark and confirm it changes polarity without changing the saved theme choice or candidate selection.
17. Confirm the instructional copy identifies Light as the more revealing tuning polarity and Dark as verification.
18. Advance through pages while watching another ClearType-rendered window; confirm compatible global settings preview as pages advance.
19. Press Back to the opening page and confirm the launch-time global settings are restored.
20. Cancel from several different pages and compare the baseline snapshot: the final state must match the launch snapshot.
21. On Finish, confirm the same selected profile is visible in side-by-side Light and Dark previews.
22. With two monitors, confirm the wizard moves to the monitor named on the page.
23. Verify both all-monitor and single-monitor calibration.
24. Verify landscape and portrait monitors.
25. Finish a session and confirm the selected per-display values and global contrast are written.
26. Disconnect a secondary monitor before Finish and confirm it is skipped with a warning.
27. Force a registry write failure in a disposable test account and verify the baseline is restored.
28. Run the stock ClearType tuner afterward and confirm it can read and supersede the settings normally.

Do not publish a release solely from a Linux portable-core build. A Windows MSVC build and manual display test are required.
