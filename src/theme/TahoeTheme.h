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

// Caption "more" button (top-right disc with a chevron-down glyph).
//
// Apple Tahoe uses a perfectly round button matching the diameter of a
// traffic light, with an always-visible faint dark fill and a 1pt
// hairline outline that mirrors the window border. Hover lifts the fill
// a touch; press lifts it slightly more.
//
// Width == Height == diameter. The right inset is tighter than the
// traffic-lights' left inset by design - Apple anchors the toolbar
// affordance closer to the corner so the chrome reads asymmetrically
// (chunkier on the close-side, leaner on the far edge).
//
// kCaptionButtonOffsetY nudges the disc downward by a fraction of a pt
// to compensate for the squircle's top corner curve - without it the
// button looks slightly too high relative to the visual centre of the
// caption strip.
inline constexpr float kCaptionButtonDiameter = 28.0f;
inline constexpr float kCaptionButtonInsetX   = 3.0f;
inline constexpr float kCaptionButtonOffsetY  = 1.0f;

// Caption "more" menu - the popup that grows out of the button.
//
// Width is fixed at 220pt so labels don't reflow as we add/remove items.
// Height is derived from the items list at layout time. The corner
// radius matches the window radius so the menu reads as the same chrome
// family rather than a foreign element.
inline constexpr float kCaptionMenuWidth         = 220.0f;
inline constexpr float kCaptionMenuRowHeight     = 30.0f;
inline constexpr float kCaptionMenuPaddingX      = 6.0f;   // outer panel
inline constexpr float kCaptionMenuPaddingY      = 6.0f;
inline constexpr float kCaptionMenuRowPaddingX   = 10.0f;  // inside rows
inline constexpr float kCaptionMenuGap           = 0.0f;   // rows are flush
inline constexpr float kCaptionMenuIconSize      = 14.0f;
inline constexpr float kCaptionMenuIconGap       = 8.0f;
inline constexpr float kCaptionMenuTextSize      = 13.0f;
inline constexpr float kCaptionMenuCornerRadius  = kWindowCornerRadius;  // 16pt
inline constexpr float kCaptionMenuRowRadius     = 7.0f;   // hover row pill
inline constexpr float kCaptionMenuAnchorGap     = 6.0f;
inline constexpr float kCaptionMenuShadowOffsetY = 8.0f;
inline constexpr float kCaptionMenuShadowBlur    = 20.0f;
inline constexpr float kCaptionMenuShadowAlpha   = 0.30f;

// Animation duration for opening/closing the menu, in milliseconds.
// 220ms is the Apple-stock spring constant for popovers - long enough
// to feel deliberate, short enough not to delay the user.
inline constexpr int kCaptionMenuAnimDurationMs = 220;

// ---- Alert dialog (in-window) ---------------------------------------------
//
// macOS Tahoe replaces native NSAlert with a Liquid Glass sheet that
// drops down inside the host window. We render exactly the same: a
// dark scrim covers the terminal area, a rounded panel fades / scales
// in on top, and the primary button is highlighted in the system
// accent. This keeps the chrome 100% ours - no DWM dialog, no
// MessageBoxW, no foreign window decorations.

inline constexpr float kAlertWidth          = 340.0f;
inline constexpr float kAlertCornerRadius   = 14.0f;
inline constexpr float kAlertPaddingX       = 22.0f;
inline constexpr float kAlertPaddingY       = 22.0f;
inline constexpr float kAlertTitleSize      = 15.0f;
inline constexpr float kAlertMessageSize    = 13.0f;
inline constexpr float kAlertTitleGap       = 6.0f;     // title -> message
inline constexpr float kAlertMessageGap     = 16.0f;    // message -> button
inline constexpr float kAlertButtonHeight   = 28.0f;
inline constexpr float kAlertButtonRadius   = 7.0f;
inline constexpr float kAlertButtonTextSize = 13.0f;
inline constexpr int   kAlertAnimDurationMs = 220;

// ---- Settings sheet (in-window) ------------------------------------------
//
// macOS Tahoe System Settings - sidebar on the left, content pane on the
// right, both rounded, with breathing space between. We render exactly
// the same structure inside the squircle. Sidebar items are pill rows
// with an SF Symbol-style glyph + label; the active row gets the system
// accent fill.

inline constexpr float kSettingsSidebarWidth      = 200.0f;
inline constexpr float kSettingsContentMinWidth   = 360.0f;
inline constexpr float kSettingsOuterPadding      = 12.0f;   // gutter inside squircle
inline constexpr float kSettingsPaneCornerRadius  = 12.0f;   // sidebar + content panes
inline constexpr float kSettingsRowHeight         = 30.0f;
inline constexpr float kSettingsRowGap            =  2.0f;
inline constexpr float kSettingsRowPaddingX       =  8.0f;
inline constexpr float kSettingsRowRadius         =  7.0f;
inline constexpr float kSettingsRowIconSize       = 14.0f;
inline constexpr float kSettingsRowIconGap        =  9.0f;
inline constexpr float kSettingsRowTextSize       = 13.0f;
inline constexpr float kSettingsHeaderTextSize    = 11.0f;
inline constexpr float kSettingsHeaderPaddingX    = 12.0f;
inline constexpr float kSettingsHeaderTopGap      = 10.0f;   // before first header
inline constexpr float kSettingsHeaderBottomGap   =  4.0f;
inline constexpr float kSettingsContentPaddingX   = 24.0f;
inline constexpr float kSettingsContentPaddingY   = 22.0f;
inline constexpr float kSettingsTitleSize         = 22.0f;   // pane title
inline constexpr float kSettingsBodyTextSize      = 13.0f;
inline constexpr float kSettingsCloseDiameter     = 22.0f;   // small circular X
inline constexpr float kSettingsCloseInsetX       =  8.0f;   // from sidebar right
inline constexpr float kSettingsCloseInsetY       =  8.0f;
inline constexpr int   kSettingsAnimDurationMs    = 280;

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
    Color captionButtonFill;    // base translucent fill always visible
    Color captionButtonGlyph;   // chevron stroke colour, active window
    Color captionButtonHover;   // translucent overlay added on hover
    Color captionButtonPressed; // slightly stronger overlay on press

    // Caption "more" menu (Liquid Glass dropdown).
    Color captionMenuFill;          // panel base fill
    Color captionMenuRowHover;      // translucent overlay for hovered row
    Color captionMenuText;          // label + icon colour
    Color captionMenuTextMuted;     // for "About" footer style if needed

    // In-window alert / dialog (replaces native MessageBoxW). Drawn in
    // our own chrome so dialogs feel like a continuation of the window
    // rather than a foreign Win32 surface.
    Color alertScrim;        // semi-transparent dim layer over the terminal
    Color alertPanelFill;    // dialog body
    Color alertTitle;        // title text
    Color alertMessage;      // body text
    Color alertButtonFill;   // primary button base
    Color alertButtonHover;  // primary button hover overlay
    Color alertButtonText;   // primary button glyph

    // In-window Settings sheet (Tahoe System Settings clone).
    Color settingsScrim;        // dim layer over terminal while open
    Color settingsSidebarFill;  // left rounded pane
    Color settingsContentFill;  // right rounded pane
    Color settingsHeader;       // section header text colour ("General", etc.)
    Color settingsRowHover;     // hovered sidebar row overlay
    Color settingsRowActive;    // selected sidebar row fill (system accent)
    Color settingsRowText;      // sidebar row label
    Color settingsRowTextActive;// selected sidebar row label
    Color settingsTitle;        // big pane title ("Appearance")
    Color settingsBody;         // pane body text
    Color settingsBodyMuted;    // secondary body text (descriptions)
    Color settingsCloseFill;    // top-right close-X background
    Color settingsCloseGlyph;   // close-X stroke

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

    .captionButtonFill    = Color::FromARGB(0x40000000),
    .captionButtonGlyph   = Color::FromARGB(0xCCEDEDEF),
    .captionButtonHover   = Color::FromARGB(0x40FFFFFF),
    .captionButtonPressed = Color::FromARGB(0x55FFFFFF),

    .captionMenuFill         = Color::FromARGB(0xF02A2A2E),
    .captionMenuRowHover     = Color::FromARGB(0x33FFFFFF),
    .captionMenuText         = Color::FromARGB(0xFFEDEDEF),
    .captionMenuTextMuted    = Color::FromARGB(0x99EDEDEF),

    // System-blue accent in dark mode is brighter than in light to keep
    // contrast against the panel fill.
    .alertScrim       = Color::FromARGB(0x80000000),
    .alertPanelFill   = Color::FromARGB(0xF02C2C30),
    .alertTitle       = Color::FromARGB(0xFFEDEDEF),
    .alertMessage     = Color::FromARGB(0xCCEDEDEF),
    .alertButtonFill  = Color::FromARGB(0xFF0A84FF),
    .alertButtonHover = Color::FromARGB(0x22FFFFFF),
    .alertButtonText  = Color::FromARGB(0xFFFFFFFF),

    .settingsScrim         = Color::FromARGB(0x99000000),
    .settingsSidebarFill   = Color::FromARGB(0xFF222226),
    .settingsContentFill   = Color::FromARGB(0xFF1F1F23),
    .settingsHeader        = Color::FromARGB(0x99EDEDEF),
    .settingsRowHover      = Color::FromARGB(0x22FFFFFF),
    .settingsRowActive     = Color::FromARGB(0xFF0A84FF),
    .settingsRowText       = Color::FromARGB(0xFFEDEDEF),
    .settingsRowTextActive = Color::FromARGB(0xFFFFFFFF),
    .settingsTitle         = Color::FromARGB(0xFFEDEDEF),
    .settingsBody          = Color::FromARGB(0xFFEDEDEF),
    .settingsBodyMuted     = Color::FromARGB(0x99EDEDEF),
    .settingsCloseFill     = Color::FromARGB(0x33FFFFFF),
    .settingsCloseGlyph    = Color::FromARGB(0xFFEDEDEF),

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

    .captionButtonFill    = Color::FromARGB(0x14000000),
    .captionButtonGlyph   = Color::FromARGB(0xCC1A1A1C),
    .captionButtonHover   = Color::FromARGB(0x14000000),
    .captionButtonPressed = Color::FromARGB(0x22000000),

    .captionMenuFill         = Color::FromARGB(0xF0F4F4F6),
    .captionMenuRowHover     = Color::FromARGB(0x14000000),
    .captionMenuText         = Color::FromARGB(0xFF1A1A1C),
    .captionMenuTextMuted    = Color::FromARGB(0x991A1A1C),

    .alertScrim       = Color::FromARGB(0x66000000),
    .alertPanelFill   = Color::FromARGB(0xF2F4F4F6),
    .alertTitle       = Color::FromARGB(0xFF1A1A1C),
    .alertMessage     = Color::FromARGB(0xCC1A1A1C),
    .alertButtonFill  = Color::FromARGB(0xFF007AFF),
    .alertButtonHover = Color::FromARGB(0x14000000),
    .alertButtonText  = Color::FromARGB(0xFFFFFFFF),

    .settingsScrim         = Color::FromARGB(0x66000000),
    .settingsSidebarFill   = Color::FromARGB(0xFFEFEFF1),
    .settingsContentFill   = Color::FromARGB(0xFFF8F8F8),
    .settingsHeader        = Color::FromARGB(0x991A1A1C),
    .settingsRowHover      = Color::FromARGB(0x14000000),
    .settingsRowActive     = Color::FromARGB(0xFF007AFF),
    .settingsRowText       = Color::FromARGB(0xFF1A1A1C),
    .settingsRowTextActive = Color::FromARGB(0xFFFFFFFF),
    .settingsTitle         = Color::FromARGB(0xFF1A1A1C),
    .settingsBody          = Color::FromARGB(0xFF1A1A1C),
    .settingsBodyMuted     = Color::FromARGB(0x991A1A1C),
    .settingsCloseFill     = Color::FromARGB(0x14000000),
    .settingsCloseGlyph    = Color::FromARGB(0xFF1A1A1C),

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
