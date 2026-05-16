# Vrtx Term

Native terminal emulator for Windows 10 / 11.

A C++ terminal that gives Windows a clean, modern look: translucent backdrop,
continuous-corner (squircle) window outline, real-time blur, traffic-light
window controls, and an in-window Settings sheet — all drawn by the
application, with no DWM-painted chrome.

## Status

Pre-alpha. The window chrome is in. The terminal core (ConPTY + VT parser)
runs; the glyph atlas, tabs, and profiles are next.

| Feature | Status |
|---|---|
| Borderless window with manual non-client logic | done |
| Squircle window outline (16pt titlebar radius) | done |
| Acrylic blur backdrop, squircle-clipped via window region | done |
| Traffic lights with group-hover and close/min/max actions | done |
| Caption "more" menu (translucent dropdown) | done |
| In-window Settings sheet (sidebar + grouped cards) | done |
| In-window alert dialog (replaces MessageBoxW) | done |
| DPI-aware rendering (PerMonitor v2) | done |
| Drop shadow via DWM extended frame | done |
| ConPTY shell session | done |
| ANSI / VT100 / xterm escape codes | done |
| GPU-accelerated glyph atlas | done |
| Tabs, splits, profiles | 1/3 done |

## Building

See [docs/BUILD.md](docs/BUILD.md) for prerequisites and step-by-step
instructions.

Quick version (with VS 2022 Build Tools installed):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\bin\Release\VrtxTerm.exe
```

## Design notes

### Squircle corners

Continuous (squircle) corners are not circular arcs. They are
superellipse-like curves with G2 (curvature) continuity at the join with the
straight edge, giving the corner a smooth, "flowing" appearance. We use the
well-known continuous-corner construction: each corner is composed of three
cubic Bezier segments (smooth-out, central arc-approximation, smooth-in),
parameterised by `radius` and `smoothing` (constant 0.6 in this project).

Corner radii used in the project:

| Window class | Radius | Used for |
|---|---|---|
| Titlebar window | 16pt | Vrtx Term MVP |
| Compact toolbar window | 20pt | future |
| Toolbar window | 26pt | future (when tabs / toolbar land) |

### Translucent backdrop

Built from three stacked layers:

1. **Acrylic blur** applied to the window via
   `SetWindowCompositionAttribute` with `ACCENT_ENABLE_ACRYLICBLURBEHIND` —
   works uniformly on Windows 10 and 11 without relying on Mica or a DWM
   backdrop type.
2. **Tint overlay** drawn by the renderer through D2D using
   `theme.windowTint` (cool slate on dark, warm white on light), with
   squircle clipping so the blur visually matches the geometry.
3. **Subtle noise** (planned) to break up uniform tint and approximate a
   frosted-glass look.

The window region (`SetWindowRgn`) is set to a polygonal approximation of the
squircle so the system-level acrylic respects the same shape.

### Why not WinUI 3 / DWM Mica?

- Mica is Win11-only and locked to a fixed corner radius / shape.
- DWM has no notion of squircle geometry.
- We want pixel-level control over tint, noise, and blend modes, and a
  consistent look across Win10 and Win11.

## License

TBD.
