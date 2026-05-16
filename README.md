# MacTermWin

macOS Tahoe-styled terminal for Windows 10 / 11.

A native C++ terminal that ports the visual language of macOS 26 (Tahoe) — Liquid Glass
backgrounds, squircle (G2-continuous) corners, real-time blur — to Windows, drawn entirely
by the application instead of relying on DWM presets.

## Status

Pre-alpha. Currently only the window chrome is implemented. The terminal core (ConPTY, VT
parser, glyph atlas) is the next milestone.

| Feature | Status |
|---|---|
| Borderless window with manual non-client logic | done |
| Squircle window outline (Apple `.continuous` style, 16pt titlebar radius) | done |
| Acrylic blur backdrop, squircle-clipped via window region | done |
| Traffic lights with group-hover and close/min/max actions | done |
| DPI-aware rendering (PerMonitor v2) | done |
| Drop shadow via DWM extended frame | done |
| ConPTY shell session | done |
| ANSI / VT100 / xterm escape codes | done |
| GPU-accelerated glyph atlas | planned |
| Tabs, splits, profiles | post-MVP |

## Building

See [docs/BUILD.md](docs/BUILD.md) for prerequisites and step-by-step instructions.

Quick version (with VS 2022 Build Tools installed):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\bin\Release\MacTermWin.exe
```

## Design notes

### Squircle corners

Apple's continuous corners are not circular arcs. They are superellipse-like curves with
G2 (curvature) continuity at the join with the straight edge, giving the corner a smooth,
"flowing" appearance. We port the figma-squircle / SwiftUI `RoundedRectangle(.continuous)`
algorithm: each corner is composed of three cubic Bezier segments (smooth-out, central
arc-approximation, smooth-in), parameterized by `radius` and `smoothing`.

Corner radii follow Apple's published spec:

| Apple window class | Radius | Used for |
|---|---|---|
| Titlebar window | 16pt | MacTermWin MVP (Terminal-equivalent) |
| Compact toolbar window | 20pt | future |
| Toolbar window | 26pt | future (when tabs/toolbar land) |

### Liquid Glass backdrop

Built from three stacked layers:

1. **Acrylic blur**, applied to the window via `SetWindowCompositionAttribute` with
   `ACCENT_ENABLE_ACRYLICBLURBEHIND` — works uniformly on Windows 10 and 11 without
   relying on Mica/DWM Backdrop type.
2. **Tint overlay**, drawn by us through D2D using `theme.windowTint` (cool slate on dark,
   warm white on light), with squircle clipping so the blur visually matches the
   geometry.
3. **Subtle noise**, planned, to break up uniform tint and feel closer to Apple's frosted
   glass.

The window region (`SetWindowRgn`) is set to a polygonal approximation of the squircle so
that the system-level acrylic respects the same shape.

### Why not just use WinUI 3 / DWM Mica?

Because:
- Mica is Win11-only and locked to a fixed corner radius / shape.
- DWM has no notion of squircle geometry.
- We want pixel-level control over tint, noise, and blend modes to match Apple's exact
  look across both Win10 and Win11.

## License

TBD. 
by kirayxa2-Vortex
