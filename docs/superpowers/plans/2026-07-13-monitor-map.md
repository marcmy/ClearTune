# Monitor Map Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the welcome-screen monitor dropdown with an OG-style clickable map that reflects the real Windows display arrangement, shows hover and selected outlines, and automatically switches to single-monitor tuning when a display is clicked.

**Architecture:** Add a portable monitor-layout helper to normalize desktop coordinates into a bounded drawing surface and support hit testing. Add a focused Win32 owner-drawn monitor-map control that renders labels and monitor silhouettes, tracks hover/focus/clicks, and reports selection changes to `MainWindow`. Keep the existing tuning model and final apply path unchanged.

**Tech Stack:** C++20, Win32/GDI, `SetWindowSubclass`, CMake/CTest.

## Global Constraints

- Native C++/Win32 only; no new dependencies.
- Preserve System/Light/Dark theming and DPI scaling.
- Clicking a monitor while “Tune all” is active must switch to “Tune one monitor.”
- Selected monitor uses a thicker blue outline; hovered monitor uses a thinner blue outline.
- The diagram must preserve relative monitor positions, proportions, and portrait/landscape orientation.
- Keyboard focus and Left/Right arrow selection must remain available.

---

### Task 1: Portable monitor layout

**Files:**
- Create: `src/core/MonitorLayout.h`
- Create: `src/core/MonitorLayout.cpp`
- Modify: `tests/CoreTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] Add failing tests for proportional fitting, relative positioning, and hit testing.
- [ ] Run CI and confirm the core test fails because the layout API is missing.
- [ ] Implement `BuildMonitorLayout` and `HitTestMonitorLayout`.
- [ ] Run portable tests and confirm they pass.

### Task 2: Native monitor-map control

**Files:**
- Create: `src/win32/MonitorMapControl.cpp`
- Modify: `src/win32/MainWindow.h`
- Modify: `src/win32/MainWindowControls.cpp`
- Modify: `src/win32/MainWindow.cpp`
- Modify: `src/win32/MainWindowView.cpp`
- Modify: `src/win32/MainWindowNavigation.cpp`
- Modify: `CMakeLists.txt`

- [ ] Add an owner-drawn, subclassed monitor-map button.
- [ ] Render real arrangement rectangles, labels, selected/hover/focus states, and dark/light surfaces.
- [ ] Map clicks to monitor indices, switch to Tune One, and keep radio state synchronized.
- [ ] Support Left/Right keyboard selection and invalidate on theme/DPI changes.
- [ ] Remove the redundant dropdown from the visible welcome layout.

### Task 3: Verification and packaging

**Files:**
- Modify: `README.md`
- Modify: `docs/testing.md`

- [ ] Document the monitor map and click-to-select behavior.
- [ ] Run portable and Windows x64 CI.
- [ ] Download and inspect the packaged artifact.
- [ ] Open a focused PR against `main`.
