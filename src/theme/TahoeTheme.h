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

// ---- Drop shadow (planned) -------------------------------------------------
//
// macOS windows are not flat - the floating glass feel comes from a soft
// drop shadow cast under the squircle. We will add it in the next iteration
// as a D2D shadow effect rendered behind the squircle.

// ---- Chrome metrics (logical pt) -------------------------------------------
//
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

// Default initial window size in logical pt.
inline constexpr int kDefaultWindowWidth  = 880;
inline constexpr int kDefaultWindowHeight = 560;

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

    // Text
    Color text;
    Color textMuted;
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

    .text      = Color::FromARGB(0xFFEDEDEF),
    .textMuted = Color::FromARGB(0x99EDEDEF),
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

    .text      = Color::FromARGB(0xFF1A1A1C),
    .textMuted = Color::FromARGB(0x991A1A1C),
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
