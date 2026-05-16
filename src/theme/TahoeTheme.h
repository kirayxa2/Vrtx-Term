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
// macOS 26 Tahoe System Settings, layout (no scrim, no blur — the sheet
// completely replaces the terminal area while it's up):
//
//   +--squircle (caption strip stays at top)---------------------------+
//   | TL  TL  TL                                              [Done]   |  <- caption
//   |                                                                  |
//   |  +--sidebar pill (r=window=16)-+                                  |
//   |  |  General                    |     Title                       |
//   |  |  Appearance                 |                                  |
//   |  |  Prompt                     |     <controls land here, on     |
//   |  |  Terminal                   |      the bare squircle - no     |
//   |  |  About                      |      card / wrapper>            |
//   |  +-----------------------------+                                  |
//   +------------------------------------------------------------------+
//
//   * Sidebar is a separate floating rounded pill. Its corner radius
//     equals the window's (16pt) - the user explicitly asked for the
//     same accuracy.
//   * Content is the bare squircle to the right with a small gutter
//     between it and the sidebar. No card / wrapper is drawn under
//     the controls; future toggles / pickers / sliders will sit
//     directly on the window surface, like Apple's panes.
//   * Done is a system-blue pill in the top-right of the caption
//     strip, replacing the chevron-button while the sheet is up.

// Sidebar pill is a tall floating capsule whose TOP edge sits flush
// against the squircle top (with a hair gap), so the traffic-lights end
// up *inside* the pill - they read as part of the sidebar's chrome,
// exactly like Apple's Tahoe System Settings. The pill therefore has
// to start at y = squircle.top + kSettingsOuterPaddingTop, not below
// the caption strip.
inline constexpr float kSettingsSidebarWidth      = 200.0f;
inline constexpr float kSettingsOuterPaddingLeft  =  8.0f;   // squircle left -> sidebar left
inline constexpr float kSettingsOuterPaddingRight = 12.0f;   // squircle right -> content right
inline constexpr float kSettingsOuterPaddingTop   =  6.0f;   // squircle top -> sidebar top
inline constexpr float kSettingsOuterPaddingBot   =  6.0f;   // squircle bottom -> sidebar bottom (symmetric with top)

// Extra horizontal shift applied to the traffic-lights when the
// Settings sheet is open, so they sit clear of the sidebar pill's
// rounded top-left corner. Without this they overlap the visual
// curvature of the corner and read as "stuck" on it.
//
// Value is the number of logical pt the disc row slides to the right
// past its normal kTrafficLightInsetX. 12pt lands the close disc just
// outside the pill's 16pt corner curve when the pill starts at
// kSettingsOuterPaddingLeft (8pt).
inline constexpr float kSettingsTrafficShiftX     = 12.0f;
inline constexpr float kSettingsSidebarGap        = 12.0f;   // sidebar pill -> content
inline constexpr float kSettingsSidebarRadius     = kWindowCornerRadius; // 16pt - matches window
inline constexpr float kSettingsRowHeight         = 32.0f;
inline constexpr float kSettingsRowGap            =  2.0f;
inline constexpr float kSettingsRowPaddingX       =  8.0f;   // pill side padding
inline constexpr float kSettingsRowRadius         =  7.0f;
inline constexpr float kSettingsRowSidePadding    = 10.0f;   // sidebar -> pill edges
inline constexpr float kSettingsTileSize          = 22.0f;   // icon tile
inline constexpr float kSettingsTileRadius        =  5.0f;
inline constexpr float kSettingsTileGlyphSize     = 13.0f;
inline constexpr float kSettingsTileTextGap       = 10.0f;
inline constexpr float kSettingsRowTextSize       = 13.0f;

// Inside the pill, the traffic-lights "live" in a reserved band on top.
// Tabs (sidebar rows) start BELOW that band with a small breathing gap,
// so the lights and the rows never overlap. The band height is
// kCaptionHeight (= the same vertical strip as the caption); the gap
// after it is `kSettingsRowsTopGap`.
inline constexpr float kSettingsRowsTopGap        =  8.0f;   // traffic band -> first row
inline constexpr float kSettingsSidebarBottomGap  = 12.0f;
inline constexpr float kSettingsContentPaddingX   = 12.0f;   // gutter inside content area
inline constexpr float kSettingsContentPaddingTop = 14.0f;   // caption -> title
inline constexpr float kSettingsTitleSize         = 28.0f;   // pane title
inline constexpr float kSettingsTitleBottomGap    = 16.0f;
inline constexpr float kSettingsBodyTextSize      = 13.0f;
inline constexpr float kSettingsDoneWidth         = 88.0f;   // primary Done pill
inline constexpr float kSettingsDoneHeight        = 22.0f;   // matches caption-button vertical centre
inline constexpr float kSettingsDoneTextSize      = 13.0f;
inline constexpr float kSettingsDoneInsetX        =  8.0f;   // squircle right -> Done right
inline constexpr int   kSettingsAnimDurationMs    = 260;

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
    //
    // The sheet replaces the terminal: there's no scrim or blur. The
    // sidebar is a separate floating rounded pill (radius == window
    // radius); the content area is the bare squircle with a small
    // gutter. Tile colours are the canonical SF Symbols-tinted
    // backgrounds Apple uses for category icons.
    Color settingsSidebarFill;  // floating sidebar pill fill
    Color settingsRowHover;     // hovered sidebar row overlay
    Color settingsRowActive;    // selected sidebar row fill (system accent)
    Color settingsRowText;      // sidebar row label
    Color settingsRowTextActive;// selected sidebar row label
    Color settingsTitle;        // big pane title ("Appearance")
    Color settingsBody;         // pane body text
    Color settingsBodyMuted;    // secondary body text (descriptions)
    Color settingsTileGlyph;    // glyph drawn on top of the icon tile
    Color settingsDoneFill;     // primary Done button (caption strip)
    Color settingsDoneHover;    // Done hover overlay (additive)
    Color settingsDoneText;     // Done glyph colour

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

    .settingsSidebarFill   = Color::FromARGB(0xFF26262A),
    .settingsRowHover      = Color::FromARGB(0x1AFFFFFF),
    .settingsRowActive     = Color::FromARGB(0xFF0A84FF),
    .settingsRowText       = Color::FromARGB(0xFFEDEDEF),
    .settingsRowTextActive = Color::FromARGB(0xFFFFFFFF),
    .settingsTitle         = Color::FromARGB(0xFFEDEDEF),
    .settingsBody          = Color::FromARGB(0xFFEDEDEF),
    .settingsBodyMuted     = Color::FromARGB(0x99EDEDEF),
    .settingsTileGlyph     = Color::FromARGB(0xFFFFFFFF),
    .settingsDoneFill      = Color::FromARGB(0xFF0A84FF),
    .settingsDoneHover     = Color::FromARGB(0x22FFFFFF),
    .settingsDoneText      = Color::FromARGB(0xFFFFFFFF),

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

    .settingsSidebarFill   = Color::FromARGB(0xFFEDEDEF),
    .settingsRowHover      = Color::FromARGB(0x10000000),
    .settingsRowActive     = Color::FromARGB(0xFF007AFF),
    .settingsRowText       = Color::FromARGB(0xFF1A1A1C),
    .settingsRowTextActive = Color::FromARGB(0xFFFFFFFF),
    .settingsTitle         = Color::FromARGB(0xFF1A1A1C),
    .settingsBody          = Color::FromARGB(0xFF1A1A1C),
    .settingsBodyMuted     = Color::FromARGB(0x991A1A1C),
    .settingsTileGlyph     = Color::FromARGB(0xFFFFFFFF),
    .settingsDoneFill      = Color::FromARGB(0xFF007AFF),
    .settingsDoneHover     = Color::FromARGB(0x14000000),
    .settingsDoneText      = Color::FromARGB(0xFFFFFFFF),

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
