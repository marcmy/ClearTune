# Reverse-engineering notes

## Scope and clean-room rule

The Windows files supplied for compatibility research are **not** part of this repository and must never be committed or redistributed. Analysis was limited to observable PE metadata, imports, resources, strings, disassembly, and user-scoped settings behavior. The application is an original implementation built on public Windows interfaces.

Analyzed file fingerprints:

| File | SHA-256 |
|---|---|
| `Windows/System32/cttune.exe` | `ed5e10ce65092374571125fefe919cc9f7444b208f7bd2444f047a0dabf57957` |
| `Windows/SysWOW64/cttune.exe` | `8bb2edc9da97652900de9a8d116b0eb0c3eb435b960be7bdb60ab45e8263acba` |
| `Windows/System32/en-US/cttune.exe.mui` | `ddf40166747518c05866250266dbe56016bea0cbe03bdd464b312a915ac1376f` |
| `Windows/SystemResources/cttune.exe.mun` | `16ddacb9497ebd2c1922827caeaa555c62d65c988d599a0cbf054241fc040326` |

## Confirmed observations

- Both executable variants are native Win32 PE files.
- The stock application imports GDI text and bitmap APIs including `CreateFontIndirectW`, `SetTextColor`, `SetBkColor`, and `DrawTextW`.
- It calls `SystemParametersInfoW` for:
  - font smoothing enabled state;
  - smoothing type;
  - smoothing contrast;
  - RGB/BGR orientation.
- Per-display values are stored beneath:

  ```text
  HKCU\Software\Microsoft\Avalon.Graphics
  ```

- Recognized value names include:
  - `GammaLevel`
  - `PixelStructure`
  - `ClearTypeLevel`
  - `TextContrastLevel`
  - `EnhancedContrastLevel`
  - `GrayscaleEnhancedContrastLevel`
- The localized wizard resources describe five sample stages with candidate counts **2, 3, 6, 6, 6**.
- Calibration text is generated at runtime rather than embedded as a set of sample bitmaps.
- The `.mun` contains decorative resources, not the calibration text itself.
- The 32-bit and 64-bit variants expose equivalent high-level behavior.

## Public SPI mapping

The implementation uses the documented `SystemParametersInfoW` actions corresponding to the behavior observed in the binary:

| Purpose | Get | Set |
|---|---:|---:|
| Font smoothing enabled | `SPI_GETFONTSMOOTHING` | `SPI_SETFONTSMOOTHING` |
| Smoothing type | `SPI_GETFONTSMOOTHINGTYPE` | `SPI_SETFONTSMOOTHINGTYPE` |
| Contrast | `SPI_GETFONTSMOOTHINGCONTRAST` | `SPI_SETFONTSMOOTHINGCONTRAST` |
| RGB/BGR orientation | `SPI_GETFONTSMOOTHINGORIENTATION` | `SPI_SETFONTSMOOTHINGORIENTATION` |

## Values inferred rather than copied

The exact private candidate arrays used by the Microsoft wizard were not recovered with enough confidence to claim a byte-for-byte match. Version 0.1 intentionally uses conservative ranges that are valid for DirectWrite custom rendering parameters:

- Pixel structure: `RGB`, `BGR`
- Gamma: `1.4`, `1.8`, `2.2`
- ClearType level: `0`, `20`, `40`, `60`, `80`, `100`
- Text contrast level: `0`, `1`, `2`, `3`, `4`, `6`
- Enhanced contrast: `0`, `0.5`, `1.0`, `1.5`, `2.0`, `3.0`

These preserve the stock candidate counts and produce meaningfully different samples without mutating live settings during the wizard.

## Why DirectWrite is used for the clone

The stock tool's GDI pipeline can evaluate one live system configuration at a time. This project needs multiple candidate cards on the same page, each with independent rendering parameters, while guaranteeing zero system mutation before Finish. DirectWrite's custom rendering parameters provide that isolation cleanly. The shell remains native Win32 and the final settings are written in Windows-compatible formats.

## Empirical follow-up

`tools/Snapshot-ClearType.ps1` captures global SPI and per-display Avalon values. A future compatibility pass can run that tool before and after controlled stock-tuner selection paths, compare snapshots, and refine candidate mappings without changing the application architecture.
