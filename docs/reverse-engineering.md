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

- Both executable variants are native Win32 PE files and expose equivalent high-level behavior.
- Calibration text is generated at runtime rather than embedded as sample bitmaps.
- The `.mun` contains decorative resources, not the calibration text itself.
- The modern sample renderer uses DirectWrite GDI interop, `IDWriteBitmapRenderTarget1`, and `IDWriteFactory1::CreateCustomRenderingParams`.
- Modern stock samples use Calibri at 11 points, with a Verdana fallback path.
- The final calibration page explicitly switches its bitmap target to grayscale antialiasing.
- The application calls `SystemParametersInfoW` for:
  - font smoothing enabled state;
  - smoothing type;
  - smoothing contrast;
  - RGB/BGR orientation.
- Those global settings are applied without persistence while moving through the wizard, followed by a desktop redraw. Cancel restores the captured values; Finish persists them.
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

## Recovered stock page sequence

The first selected monitor uses five calibration pages:

| Page | Parameter | Scope | Choices |
|---:|---|---|---:|
| 1 | Pixel structure | Per monitor | 2 |
| 2 | Font-smoothing contrast / DirectWrite gamma | **Global** | 6 |
| 3 | ClearType level | Per monitor | 3 |
| 4 | Enhanced/text contrast combination | Per monitor | 6 |
| 5 | Grayscale enhanced contrast | Per monitor | 6 |

Stock `cttune.exe` skips page 2 for every subsequent monitor. Disassembly confirms that the selected value is stored in the global working-settings structure and applied with `SPI_SETFONTSMOOTHINGCONTRAST`, rather than as a monitor-local page choice. Repeating it for a second display would therefore be redundant and would misleadingly imply that Windows maintains a separate value for each monitor.

ClearTune follows the same rule: the first monitor selected in a tuning session gets five pages, while later selected monitors get four. A single-monitor session still shows the global page once, even when the selected display is not the Windows primary monitor.

## Recovered candidate tables

### Global contrast page

The six values are generated from the contrast active when the wizard begins:

- If the hundreds digit is odd: `1100, 1300, 1500, 1700, 1900, 2100`
- Otherwise, when the current value is below `1800`: `1000, 1200, 1400, 1600, 1800, 2000`
- Otherwise: `1200, 1400, 1600, 1800, 2000, 2200`

The values are passed to DirectWrite as gamma divided by `1000` and are applied globally through `SPI_SETFONTSMOOTHINGCONTRAST`.

### Per-monitor pages

- Pixel structure: `1, 2`
- ClearType level: `100, 50, 0`
- Page-4 enhanced contrast: `0, 50, 100, 200, 300, 400`
- Page-4 text contrast: `0, 0, 1, 1, 1, 2`
- Grayscale enhanced contrast: `0, 50, 100, 200, 300, 400`

Before being passed to DirectWrite, both enhanced-contrast channels are divided by `100`, and ClearType level is divided by `100`.

## Public SPI mapping

| Purpose | Get | Set |
|---|---:|---:|
| Font smoothing enabled | `SPI_GETFONTSMOOTHING` | `SPI_SETFONTSMOOTHING` |
| Smoothing type | `SPI_GETFONTSMOOTHINGTYPE` | `SPI_SETFONTSMOOTHINGTYPE` |
| Global contrast | `SPI_GETFONTSMOOTHINGCONTRAST` | `SPI_SETFONTSMOOTHINGCONTRAST` |
| RGB/BGR orientation | `SPI_GETFONTSMOOTHINGORIENTATION` | `SPI_SETFONTSMOOTHINGORIENTATION` |

ClearTune uses flag `0` for reversible in-session preview and `SPIF_UPDATEINIFILE | SPIF_SENDCHANGE` only for the final committed state.

## Rendering approach

Each card must render with independent parameters, so ClearTune follows the stock GDI-compatible DirectWrite path rather than changing the machine-wide settings once per card. It creates a bitmap render target, applies custom rendering parameters, lays out the sample text through DirectWrite, then copies the resulting bitmap into the native owner-drawn button.

The selected profile and global contrast are also applied to the compatible SPI settings when advancing through the wizard. This reproduces the stock tool's page-to-page live-preview behavior while preserving independent candidate cards.

## Polarity comparison

Light-on-dark rendering is often less revealing of weight and contrast differences than dark-on-light rendering. ClearTune does not alter or exaggerate the recovered candidate values to compensate. Instead, it preserves the stock rendering model and adds an opposite-polarity comparison plus a final dual-polarity preview. This keeps the calibration technically compatible while making dark-theme validation practical.

## Empirical follow-up

`tools/Snapshot-ClearType.ps1` captures global SPI and per-display Avalon values. It remains useful for comparing stock and ClearTune behavior across Windows builds, graphics drivers, display overrides, and unusual monitor configurations.
