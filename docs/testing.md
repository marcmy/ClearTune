# Testing guide

## Automated tests

### Portable core

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The core suite covers theme and comparison-polarity resolution, persisted theme values, display-key normalization, CRU-style preferred-resolution handling, EDID/DisplayConfig monitor-name preference, monitor-map scaling, spacing and hit testing, recovered global-contrast candidate generation, per-monitor candidate mappings, transactional Back/Next rollback, per-monitor review pages, final-review accuracy, and multi-monitor wizard navigation.

### Windows build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The Windows application target compiling cleanly is also a header/API smoke test for the Win32, DirectWrite 1, GDI interop, DisplayConfig identity, monitor-map, and settings-preview layers.

## Manual release checklist

1. Export a baseline with `tools/Snapshot-ClearType.ps1`.
2. Launch on a Windows 10 or Windows 11 x64 machine without elevation.
3. Confirm System follows the current Windows app theme.
4. Select Light, restart, and confirm Light persists.
5. Select Dark, restart, and confirm Dark persists.
6. Return to System and confirm it follows Windows again.
7. With multiple monitors, confirm the welcome map matches Windows' relative positions, proportions, and portrait/landscape orientation.
8. Confirm EDID model names such as `XZ270 X` or `BenQ GW2765` appear instead of `Generic PnP Monitor` when Windows exposes them.
9. Confirm adjacent monitor tiles have a small visual gap and hover/selection outlines have breathing room around the labels and bezels.
10. Hover each monitor and confirm it receives a thin blue outline without changing the radio selection.
11. Click a monitor while Tune All is active and confirm Tune One is selected automatically with a thicker outline around that monitor.
12. Use Left/Right and Up/Down while the map has keyboard focus and confirm selection wraps through the displays.
13. Confirm the map remains legible and correctly scaled in both Light and Dark modes and at 100%, 125%, 150%, and 200% DPI.
14. Confirm the first selected monitor has 2, 6, 3, 6, and 6 cards in order.
15. Confirm each later selected monitor skips the six-card global-contrast page and shows 2, 3, 6, and 6 cards.
16. Tune only a secondary monitor and confirm the global-contrast page still appears once.
17. On every calibration page, change the theme and confirm the current candidate selection remains unchanged.
18. Use Compare Light/Dark and confirm it changes polarity without changing the saved theme choice or candidate selection.
19. Confirm the instructional copy identifies Light as the more revealing tuning polarity and Dark as verification.
20. Advance through pages while watching another ClearType-rendered window; confirm compatible global settings preview as pages advance.
21. On pages 3, 4, and 5, choose obvious non-default samples, then press Back repeatedly and confirm each Back removes the choice committed by the corresponding Next.
22. Return from page 5 to page 2 and confirm page 2 uses its original stock baseline rather than inheriting page-4 or page-5 contrast values.
23. When tuning all monitors, confirm a side-by-side Light/Dark review appears after page 5 for each non-final monitor before the wizard advances.
24. Confirm the monitor name and dimensions on each review page match the monitor whose settings were just tuned.
25. Press Back from the next monitor and confirm the previous monitor's review page and exact selected profile are restored.
26. Confirm the final monitor's Finish page shows the exact profile that will be committed, with no global page-2 value substituted for its per-monitor gamma.
27. Press Back to the opening page and confirm the launch-time global settings are restored.
28. Cancel from several different pages and compare the baseline snapshot: the final state must match the launch snapshot.
29. With two monitors, confirm the wizard moves to the monitor named on the page.
30. Verify both all-monitor and single-monitor calibration.
31. Verify landscape and portrait monitors.
32. Finish a session and confirm the selected per-display values and global contrast are written.
33. Disconnect a secondary monitor before Finish and confirm it is skipped with a warning.
34. Force a registry write failure in a disposable test account and verify the baseline is restored.
35. Run the stock ClearType tuner afterward and confirm it can read and supersede the settings normally.

Do not publish a release solely from a Linux portable-core build. A Windows MSVC build and manual display test are required.
