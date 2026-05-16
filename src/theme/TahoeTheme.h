// Theme constants and colors that mirror macOS 26 "Tahoe".
//
// Apple publishes three window classes with three corner radii:
//
//   Toolbar window         26pt   <- has a real toolbar (Finder, Mail, Safari)
//   Compact toolbar window 20pt
//   Titlebar window        16pt   <- thin title strip; Terminal, Notes, TextEdit
//
// MacTermWin's MVP behaves like a "Titlebar window": we ship traffic
// lights in a thin caption strip but do not have a separate toolbar yet,
// so 16pt is the matching radius. When tabs and toolbar land we will
// revisit.
//
// All sizes are in `pt` == device-independent pixels (logical px @ 100% scale).
// Use ToPx() to multiply by the per-monitor DPI factor at draw time.

#pragma once

#include "pch.h"

namespace mactw::theme {

// ---- Corner radii (Apple-spec) ---------------------------------------------

inline constexpr float kRadiusTitlebarWindow       = 16.0f;
inline constexpr float kRadiusCompactToolbarWindow = 20.0f;
inline constexpr float kRadiusToolbarWindow        = 26.0f;

// Currently active class.
inline constexpr float kWindowCornerRadius = kRadiusTitlebarWindow;

// Squircle smoothing factor. Hardcoded at 0.6 (Apple `.continuous` /
// figma-squircle iOS-7 icon shape) inside SquircleGeometry.cpp via the
// published Figma coefficients.
inline constexpr float kSquircleSmoothing = 0.6f;

// ---- Chrome metrics (logical pt) -------------------------------------------

// Drop shadow under the squircle. The HWND is enlarged by kShadowMargin
// on every side so the blurred shadow has room; the squircle itself is
// drawn with offset (margin, margin) inside the HWND.
//
// Values mirror the macOS Tahoe reference SVG:
//     filter: drop-shadow(0 5pt 15pt rgba(0,0,0,0.30))
inline constexpr float kShadowMargin   = 28.0f;  // >= shadowOffsetY + 2*blur
inline constexpr float kShadowOffsetY  =  5.0f;
inline constexpr float kShadowBlurStd  = 15.0f;
inline constexpr float kShadowAlpha    =  0.30f;

// These five values were dialled in by hand against side-by-side reference
// screenshots of macOS 26 Tahoe Terminal on the live tweaker (`[`/`]` etc.,
// since removed). They are the final shipping numbers; do not "round" them
// to the Apple HIG canonical values - the HIG values look slightly off at
// our DPI / scale.

// Height of the draggable caption strip at the top of the window.
inline constexpr float kCaptionHeight = 31.0f;

// Resize border thickness for hit-testing.
inline constexpr float kResizeBorder = 6.0f;

// 1pt hairline outline drawn around the squircle, mirrors the thin light
// rim macOS puts on every window. The colour comes from
// `Palette.windowBorder` (a low-alpha white in the dark theme).
inline constexpr float kWindowBorderWidth = 1.0f;

// Traffic-lights:
//     diameter        13 pt
//     edge-to-edge     10 pt   (gap between adjacent discs)
//     left inset      11 pt    (window edge to left edge of first disc)
inline constexpr float kTrafficLightDiameter = 13.0f;
inline constexpr float kTrafficLightSpacing  = 10.0f;
inline constexpr float kTrafficLightInsetX   = 11.0f;

// Caption "more" button (top-right pill with a chevron-down glyph).
// Pill metrics roughly match the sidebar / split-view affordances on
// macOS 26 Tahoe windows. The right inset mirrors `kTrafficLightInsetX`
// so the chrome reads as left-right symmetric.
inline constexpr float kCaptionButtonWidth  = 32.0f;
inline constexpr float kCaptionButtonHeight = 22.0f;
inline constexpr float kCaptionButtonInsetX = 11.0f;

// Default initial window size in logical pt.
inline constexpr int kDefaultWindowWidth  = 880;
inline constexpr int kDefaultWindowHeight = 560;

// ---- Terminal grid metrics -------------------------------------------------

// Padding between the inner edge of the squircle and the first / last cell
// of the grid. Apple's Terminal uses a generous left/right gutter; the top
// gutter starts immediately below the caption strip.
inline constexpr float kTerminalPaddingX = 12.0f;
inline constexpr float kTerminalPaddingY =  8.0f;

// Default monospace font. Cascadia Code ships with Windows 11 and is
// installable on Windows 10. Fallback chain inside DirectWrite is handled
// automatically when the requested face is missing.
inline constexpr wchar_t kTerminalFontFamily[] = L"Cascadia Code";

// Logical font size in pt. DirectWrite expects DIPs, which is the same
// number on a 96-DPI display; we scale it manually for higher DPI.
inline constexpr float kTerminalFontSize = 13.0f;

// Line-height multiplier on top of the font's natural cell height. 1.20
// gives macOS Terminal-like breathing room without looking sparse.
inline constexpr float kTerminalLineHeight = 1.20f;

// ---- Colors ----------------------------------------------------------------

struct Color {
    float r{}, g{}, b{}, a{1.0f};

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    // Convenience: build from 0xAARRGGBB.
    static constexpr Color FromARGB(uint32_t argb) {
        return Color(((argb >> 16) & 0xFF) / 255.0f,
                     ((argb >> 8)  & 0xFF) / 255.0f,
                     ((argb >> 0)  & 0xFF) / 255.0f,
                     ((argb >> 24) & 0xFF) / 255.0f);
    }

    // For SetWindowCompositionAttribute / acrylic accent. Format: 0xAABBGGRR.
    constexpr uint32_t ToAccentABGR() const {
        auto channel = [](float v) -> uint32_t {
            v = std::clamp(v, 0.0f, 1.0f);
            return static_cast<uint32_t>(v * 255.0f + 0.5f);
        };
        return (channel(a) << 24) | (channel(b) << 16) | (channel(g) << 8) | channel(r);
    }
};

struct Palette {
    // Window
    Color windowTint;          // overlay drawn on top of acrylic
    Color windowBorder;        // 1px hairline around the squircle
    Color contentBackground;   // fallback for content area before terminal lands

    // Traffic lights
    Color tlClose;
    Color tlMinimize;
    Color tlMaximize;
    Color tlInactive;          // when window is not key/foreground
    Color tlGlyph;              // dark glyph drawn on the colored disc on hover

    // Caption "more" button (top-right pill).
    Color captionButtonGlyph;   // chevron stroke colour, active window
    Color captionButtonHover;   // translucent pill background on hover
    Color captionButtonPressed; // slightly stronger pill on press

    // Text
    Color text;
    Color textMuted;

    // ANSI 16-color palette used by the terminal grid. Indices 0..7 are the
    // standard SGR 30..37 colors, 8..15 are the bright variants (SGR 90..97
    // and bold-as-bright fallback).
    Color ansi[16];

    // Default foreground / background for the terminal grid (used when a
    // cell's `fg` / `bg` are SGR 39 / 49 -> "default"). Keeping these
    // separate from .text / .windowTint lets the terminal visually deviate
    // from the chrome (e.g. a slightly different black for the body).
    Color terminalFg;
    Color terminalBg;
    Color cursor;              // block cursor color when window is focused
};

// Apple-flavored dark palette. Values measured visually against macOS Tahoe
// reference screenshots; tweak in code, not at runtime.
//
// All alphas are 1.0 here: the MVP does not use acrylic blur, so the window
// is fully opaque. When we re-introduce blur later we'll drop the alpha on
// `windowTint` again so the backdrop shows through.
inline constexpr Palette kDarkPalette{
    .windowTint        = Color::FromARGB(0xFF1C1C1F),
    .windowBorder      = Color::FromARGB(0x33FFFFFF),
    .contentBackground = Color::FromARGB(0xFF1C1C1F),

    .tlClose    = Color::FromARGB(0xFFFF5F57),
    .tlMinimize = Color::FromARGB(0xFFFEBC2E),
    .tlMaximize = Color::FromARGB(0xFF28C840),
    .tlInactive = Color::FromARGB(0xFF595959),
    .tlGlyph    = Color::FromARGB(0xC8000000),

    .captionButtonGlyph   = Color::FromARGB(0xCCEDEDEF),
    .captionButtonHover   = Color::FromARGB(0x22FFFFFF),
    .captionButtonPressed = Color::FromARGB(0x33FFFFFF),

    .text      = Color::FromARGB(0xFFEDEDEF),
    .textMuted = Color::FromARGB(0x99EDEDEF),

    // ANSI 16: macOS Terminal "Pro" scheme, slightly desaturated for dark bg.
    .ansi = {
        Color::FromARGB(0xFF1C1C1F),  //  0 black
        Color::FromARGB(0xFFE05561),  //  1 red
        Color::FromARGB(0xFF5BC273),  //  2 green
        Color::FromARGB(0xFFE2B86B),  //  3 yellow
        Color::FromARGB(0xFF6CA8E2),  //  4 blue
        Color::FromARGB(0xFFC678DD),  //  5 magenta
        Color::FromARGB(0xFF56B6C2),  //  6 cyan
        Color::FromARGB(0xFFEDEDEF),  //  7 white  (default fg)
        Color::FromARGB(0xFF5C6370),  //  8 bright black
        Color::FromARGB(0xFFFF7B85),  //  9 bright red
        Color::FromARGB(0xFF7BD893),  // 10 bright green
        Color::FromARGB(0xFFFFD479),  // 11 bright yellow
        Color::FromARGB(0xFF89C2FF),  // 12 bright blue
        Color::FromARGB(0xFFD68FF1),  // 13 bright magenta
        Color::FromARGB(0xFF7CCEDB),  // 14 bright cyan
        Color::FromARGB(0xFFFFFFFF),  // 15 bright white
    },
    .terminalFg = Color::FromARGB(0xFFEDEDEF),
    .terminalBg = Color::FromARGB(0xFF1C1C1F),
    .cursor     = Color::FromARGB(0xCCEDEDEF),
};

// Light palette (for future "Tahoe Light" theme). Currently unused; kept here
// so the rest of the code can already reference Theme::active().
inline constexpr Palette kLightPalette{
    .windowTint        = Color::FromARGB(0xA0F4F4F6),
    .windowBorder      = Color::FromARGB(0x22000000),
    .contentBackground = Color::FromARGB(0x99FFFFFF),

    .tlClose    = Color::FromARGB(0xFFFF5F57),
    .tlMinimize = Color::FromARGB(0xFFFEBC2E),
    .tlMaximize = Color::FromARGB(0xFF28C840),
    .tlInactive = Color::FromARGB(0xFFB0B0B0),
    .tlGlyph    = Color::FromARGB(0xC8000000),

    .captionButtonGlyph   = Color::FromARGB(0xCC1A1A1C),
    .captionButtonHover   = Color::FromARGB(0x14000000),
    .captionButtonPressed = Color::FromARGB(0x22000000),

    .text      = Color::FromARGB(0xFF1A1A1C),
    .textMuted = Color::FromARGB(0x991A1A1C),

    .ansi = {
        Color::FromARGB(0xFF000000),  //  0 black
        Color::FromARGB(0xFFC2261B),  //  1 red
        Color::FromARGB(0xFF2A8A3E),  //  2 green
        Color::FromARGB(0xFFB58205),  //  3 yellow
        Color::FromARGB(0xFF1F6FEB),  //  4 blue
        Color::FromARGB(0xFFA64BCB),  //  5 magenta
        Color::FromARGB(0xFF008B94),  //  6 cyan
        Color::FromARGB(0xFFCCCCCC),  //  7 white
        Color::FromARGB(0xFF7F7F7F),  //  8 bright black
        Color::FromARGB(0xFFE5483D),  //  9 bright red
        Color::FromARGB(0xFF3FBE5C),  // 10 bright green
        Color::FromARGB(0xFFD9A300),  // 11 bright yellow
        Color::FromARGB(0xFF4287F5),  // 12 bright blue
        Color::FromARGB(0xFFC066E0),  // 13 bright magenta
        Color::FromARGB(0xFF2EBAC1),  // 14 bright cyan
        Color::FromARGB(0xFFFFFFFF),  // 15 bright white
    },
    .terminalFg = Color::FromARGB(0xFF1A1A1C),
    .terminalBg = Color::FromARGB(0xFFFAFAFC),
    .cursor     = Color::FromARGB(0xCC1A1A1C),
};

// Currently active palette. We only ship dark for the MVP; the runtime toggle
// will land with the settings UI.
const Palette& ActivePalette();

// ---- DPI helpers -----------------------------------------------------------

// Convert logical pt (== px at 96 DPI) to physical pixels at the given DPI.
inline float ToPx(float pt, UINT dpi) {
    return pt * (static_cast<float>(dpi) / 96.0f);
}

inline int ToPxInt(float pt, UINT dpi) {
    return static_cast<int>(std::lround(ToPx(pt, dpi)));
}

}  // namespace mactw::theme
