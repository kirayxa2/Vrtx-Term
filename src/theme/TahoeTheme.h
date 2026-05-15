// Theme constants and colors that mirror macOS 26 "Tahoe".
//
// Apple publishes three window classes with three corner radii:
//
//   Toolbar window         26pt
//   Compact toolbar window 20pt
//   Titlebar window        16pt
//
// MacTermWin's MVP behaves like the Terminal app, which is a "Titlebar window".
// When tabs / toolbar land we will switch to 26pt.
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

// Squircle smoothing factor. 0.0 == regular circular arc corner, 0.6 closely
// matches Apple's `.continuous` / figma-squircle output for radii up to 32pt.
inline constexpr float kSquircleSmoothing = 0.6f;

// ---- Chrome metrics (logical pt) -------------------------------------------

// Height of the draggable caption strip at the top of the window.
inline constexpr float kCaptionHeight = 38.0f;

// Resize border thickness for hit-testing. Apple windows are hit-testable a
// few pt outside their visible squircle; we keep that but stay inside the
// square bounding box.
inline constexpr float kResizeBorder = 6.0f;

// Traffic-lights: Apple uses 12pt circles, 8pt apart, 13pt from window edge.
inline constexpr float kTrafficLightDiameter = 12.0f;
inline constexpr float kTrafficLightSpacing  = 8.0f;
inline constexpr float kTrafficLightInsetX   = 13.0f;
inline constexpr float kTrafficLightInsetY   = 13.0f;

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
