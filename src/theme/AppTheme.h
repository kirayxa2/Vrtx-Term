// Theme constants and colors that mirror Vrtx Term.
//
// Three common window-chrome classes with three corner radii:
//
//   Toolbar window         26pt   <- has a real toolbar (Finder, Mail, Safari)
//   Compact toolbar window 20pt
//   Titlebar window        16pt   <- thin title strip; Terminal, Notes, TextEdit
//
// VrtxTerm's MVP behaves like a "Titlebar window": we ship traffic
// lights in a thin caption strip but do not have a separate toolbar yet,
// so 16pt is the matching radius. When tabs and toolbar land we will
// revisit.
//
// All sizes are in `pt` == device-independent pixels (logical px @ 100% scale).
// Use ToPx() to multiply by the per-monitor DPI factor at draw time.

#pragma once

#include "pch.h"

namespace vrtx::theme {

// ---- Corner radii ---------------------------------------------

inline constexpr float kRadiusTitlebarWindow       = 16.0f;
inline constexpr float kRadiusCompactToolbarWindow = 20.0f;
inline constexpr float kRadiusToolbarWindow        = 26.0f;

// Currently active class.
inline constexpr float kWindowCornerRadius = kRadiusTitlebarWindow;

// Squircle smoothing factor. Hardcoded at 0.6 (continuous-corner /
// continuous-corner icon shape) inside SquircleGeometry.cpp via the
// published continuous-corner coefficients.
inline constexpr float kSquircleSmoothing = 0.6f;

// ---- Chrome metrics (logical pt) -------------------------------------------

// Drop shadow under the squircle. The HWND is enlarged by kShadowMargin
// on every side so the blurred shadow has room; the squircle itself is
// drawn with offset (margin, margin) inside the HWND.
//
// Values mirror the our reference design:
//     filter: drop-shadow(0 5pt 15pt rgba(0,0,0,0.30))
inline constexpr float kShadowMargin   = 28.0f;  // >= shadowOffsetY + 2*blur
inline constexpr float kShadowOffsetY  =  5.0f;
inline constexpr float kShadowBlurStd  = 15.0f;
inline constexpr float kShadowAlpha    =  0.30f;

// These five values were dialled in by hand against side-by-side reference
// screenshots of our reference design on the live tweaker (`[`/`]` etc.,
// since removed). They are the final shipping numbers; do not "round" them
// to the canonical values - the HIG values look slightly off at
// our DPI / scale.

// Height of the draggable caption strip at the top of the window.
inline constexpr float kCaptionHeight = 31.0f;

// Tab strip sits in its own band below the caption, above the
// terminal grid. The native macOS Terminal uses ~28pt for this band.
// The whole strip is draggable everywhere except the tab pills and
// the "+" button.
inline constexpr float kTabStripHeight    = 28.0f;
inline constexpr float kTabStripPaddingX  =  8.0f;   // squircle edge -> first pill / "+" group
inline constexpr float kTabStripDividerY  =  0.5f;   // 1px hairline between strip and terminal

// Caption-strip title (e.g. "user — bash — 80×24"), centred horizontally.
// Sized close to the macOS native caption title (~13pt SemiBold).
inline constexpr float kCaptionTitleSize  = 13.0f;

// Resize border thickness for hit-testing.
inline constexpr float kResizeBorder = 6.0f;

// 1pt hairline outline drawn around the squircle, mirrors the thin light
// light rim that we draw on the squircle. The colour comes from
// `Palette.windowBorder` (a low-alpha white in the dark theme).
inline constexpr float kWindowBorderWidth = 1.0f;

// ---- Frosted-glass noise overlay ------------------------------------------
//
// Tiled 128x128 bitmap drawn at very low opacity twice per frame: once
// over the bare squircle (so the window itself reads as frosted), once
// over the chrome (so translucent panes inherit the same grain). The
// numbers are deliberately low - anything above ~0.06 starts to read as
// static rather than as a material grain.
inline constexpr int   kNoiseTileSize     = 128;
inline constexpr float kNoiseSurfaceAlpha = 0.040f;   // base window grain
inline constexpr float kNoiseChromeAlpha  = 0.025f;   // grain over chrome

// Traffic-lights:
//     diameter        13 pt
//     edge-to-edge     10 pt   (gap between adjacent discs)
//     left inset      11 pt    (window edge to left edge of first disc)
inline constexpr float kTrafficLightDiameter = 13.0f;
inline constexpr float kTrafficLightSpacing  = 10.0f;
inline constexpr float kTrafficLightInsetX   = 11.0f;

// Caption "more" button (top-right disc with a chevron-down glyph).
//
// We use a perfectly round button matching the diameter of a
// traffic light, with an always-visible faint dark fill and a 1pt
// hairline outline that mirrors the window border. Hover lifts the fill
// a touch; press lifts it slightly more.
//
// Width == Height == diameter. The right inset is tighter than the
// traffic-lights' left inset by design - we anchor the toolbar
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
// 220ms is the spring constant for popovers - long enough
// to feel deliberate, short enough not to delay the user.
inline constexpr int kCaptionMenuAnimDurationMs = 220;

// ---- Alert dialog (in-window) ---------------------------------------------
//
// We replace the native MessageBoxW with our own in-window sheet that
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
// the Settings sheet, layout (no scrim, no blur — the sheet
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
//     directly on the window surface, like a system Settings pane.
//   * Done is a system-blue pill in the top-right of the caption
//     strip, replacing the chevron-button while the sheet is up.

// Sidebar pill is a tall floating capsule whose TOP edge sits flush
// against the squircle top (with a hair gap), so the traffic-lights end
// up *inside* the pill - they read as part of the sidebar's chrome,
// exactly like the rest of the Settings sheet. The pill therefore has
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

// Same idea, vertically. By default the discs are centred in the
// caption strip (y = captionH/2). When Settings is open the pill's
// top edge sits at squircle.top + kSettingsOuterPaddingTop (6pt) and
// the eye reads the discs as glued to the pill top.
//
// 5pt nudges the disc centre down so there's roughly the same
// breathing room above each disc as on its sides, matching how
// the Settings sheet spaces the lights inside the
// sidebar header.
inline constexpr float kSettingsTrafficShiftY     =  5.0f;
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
inline constexpr float kSettingsTitleSize         = 22.0f;   // pane title (the Settings sheet is ~22pt)
inline constexpr float kSettingsTitleBottomGap    = 14.0f;
inline constexpr float kSettingsBodyTextSize      = 13.0f;
inline constexpr float kSettingsDoneWidth         = 88.0f;   // primary Done pill
inline constexpr float kSettingsDoneHeight        = 22.0f;   // matches caption-button vertical centre
inline constexpr float kSettingsDoneTextSize      = 13.0f;
inline constexpr float kSettingsDoneInsetX        =  8.0f;   // squircle right -> Done right
inline constexpr int   kSettingsAnimDurationMs    = 260;

// ---- Settings sidebar pill outline ----------------------------------------
//
// Mirrors the window's own 1pt hairline. Gives the pill the same
// "carved out of glass" silhouette the squircle has, instead of a
// solid silhouette that floats inside the content area.
inline constexpr float kSettingsSidebarBorderWidth = 1.0f;

// ---- Settings content cards (grouped table) -------------------------
//
// the Settings sheet groups rows into rounded "cards" stacked
// vertically. Each card has:
//   * an optional small all-caps header above (13pt, secondary text)
//   * a translucent rounded fill (radius 12pt)
//   * rows separated by a 1pt hairline that does NOT touch the card edges
//
// We model this with `Section { header, rows[] }`. The renderer walks
// the active item's sections from top to bottom inside the content
// pane.
inline constexpr float kSettingsCardRadius        = 12.0f;
inline constexpr float kSettingsCardSpacing       = 18.0f;   // between cards
inline constexpr float kSettingsCardPaddingX      = 16.0f;   // inside row, horizontal
inline constexpr float kSettingsCardRowHeight     = 44.0f;   // stock value
inline constexpr float kSettingsCardSeparatorInset= 16.0f;   // hairline left inset
inline constexpr float kSettingsCardSeparatorWidth=  1.0f;   // hairline thickness
inline constexpr float kSettingsSectionHeaderSize = 13.0f;
inline constexpr float kSettingsSectionHeaderGap  =  6.0f;   // header -> card top
inline constexpr float kSettingsRowLabelSize      = 13.0f;
inline constexpr float kSettingsRowValueSize      = 13.0f;
inline constexpr float kSettingsRowChevronSize    = 11.0f;   // the > glyph
inline constexpr float kSettingsRowChevronGap     =  6.0f;   // value -> chevron
inline constexpr float kSettingsRowEndPadding     = 14.0f;   // right edge breathing room

// Toggle pill. The's switch is 38x22 with an 18pt knob.
inline constexpr float kSettingsToggleWidth       = 38.0f;
inline constexpr float kSettingsToggleHeight      = 22.0f;
inline constexpr float kSettingsToggleKnob        = 18.0f;
inline constexpr float kSettingsToggleKnobInset   =  2.0f;   // knob -> pill edge

// Section "footer" — a small grey paragraph sometimes drawn UNDER a
// card (used for "Privacy notice" style explainers). Same
// font as the section header but slightly larger leading.
inline constexpr float kSettingsFooterTextSize    = 12.0f;
inline constexpr float kSettingsFooterTopGap      =  6.0f;   // card -> footer
inline constexpr float kSettingsFooterBottomGap   = 14.0f;   // footer -> next card

// Default initial window size in logical pt.
inline constexpr int kDefaultWindowWidth  = 880;
inline constexpr int kDefaultWindowHeight = 560;

// ---- Terminal grid metrics -------------------------------------------------

// Padding between the inner edge of the squircle and the first / last cell
// of the grid. We use a generous left/right gutter; the top
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
// gives comfortable breathing room without looking sparse.
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

    // Caption "more" menu (translucent dropdown).
    Color captionMenuFill;          // panel base fill
    Color captionMenuRowHover;      // translucent overlay for hovered row
    Color captionMenuText;          // label + icon colour
    Color captionMenuTextMuted;     // for "About" footer style if needed

    // Caption-strip centred title ("user — bash — 80×24"). Slightly
    // muted in the inactive window state; the renderer fades alpha
    // when the window is not the foreground.
    Color captionTitle;
    Color captionTitleMuted;

    // Tab strip (the band below the caption, above the terminal).
    Color tabStripBg;            // strip fill
    Color tabStripDivider;       // 1pt hairline at strip bottom

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

    // In-window Settings sheet (the Settings sheet clone).
    //
    // The sheet replaces the terminal: there's no scrim or blur. The
    // sidebar is a separate floating rounded pill (radius == window
    // radius); the content area is the bare squircle with a small
    // gutter. Tile colours are the canonical tinted/saturated
    // backgrounds typically used for category icons.
    Color settingsSidebarFill;  // floating sidebar pill fill
    Color settingsSidebarBorder;// 1pt hairline around the sidebar pill
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

    // Content cards (grouped table). Each "Section" renders as
    // one of these cards with rows inside it.
    Color settingsCardFill;       // rounded card background
    Color settingsCardBorder;     // 1pt hairline around the card
    Color settingsCardSeparator;  // 1pt hairline between rows
    Color settingsSectionHeader;  // small grey label above each card
    Color settingsRowLabel;       // primary row text
    Color settingsRowValue;       // trailing value text (e.g. "Dark")
    Color settingsRowChevron;     // disclosure chevron glyph
    Color settingsRowFooter;      // small description paragraph under cards

    // Toggle pill.
    Color settingsToggleOff;      // pill fill when off
    Color settingsToggleOn;       // pill fill when on (system green)
    Color settingsToggleKnob;     // knob colour

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

// Default dark palette. Values measured visually against Vrtx Term
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

    .captionMenuFill         = Color::FromARGB(0xCC2E2E33),
    .captionMenuRowHover     = Color::FromARGB(0x33FFFFFF),
    .captionMenuText         = Color::FromARGB(0xFFEDEDEF),
    .captionMenuTextMuted    = Color::FromARGB(0x99EDEDEF),

    .captionTitle      = Color::FromARGB(0xCCEDEDEF),
    .captionTitleMuted = Color::FromARGB(0x66EDEDEF),
    .tabStripBg        = Color::FromARGB(0x00000000),
    .tabStripDivider   = Color::FromARGB(0x00FFFFFF),

    // System-blue accent in dark mode is brighter than in light to keep
    // contrast against the panel fill.
    .alertScrim       = Color::FromARGB(0x80000000),
    .alertPanelFill   = Color::FromARGB(0xD42E2E33),
    .alertTitle       = Color::FromARGB(0xFFEDEDEF),
    .alertMessage     = Color::FromARGB(0xCCEDEDEF),
    .alertButtonFill  = Color::FromARGB(0xFF0A84FF),
    .alertButtonHover = Color::FromARGB(0x22FFFFFF),
    .alertButtonText  = Color::FromARGB(0xFFFFFFFF),

    .settingsSidebarFill   = Color::FromARGB(0x9636363C),
    .settingsSidebarBorder = Color::FromARGB(0x33FFFFFF),
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

    .settingsCardFill      = Color::FromARGB(0xA63E3E44),
    .settingsCardBorder    = Color::FromARGB(0x26FFFFFF),
    .settingsCardSeparator = Color::FromARGB(0x1FFFFFFF),
    .settingsSectionHeader = Color::FromARGB(0x99EDEDEF),
    .settingsRowLabel      = Color::FromARGB(0xFFEDEDEF),
    .settingsRowValue      = Color::FromARGB(0xB3EDEDEF),
    .settingsRowChevron    = Color::FromARGB(0x80EDEDEF),
    .settingsRowFooter     = Color::FromARGB(0x80EDEDEF),

    .settingsToggleOff     = Color::FromARGB(0xFF48484A),
    .settingsToggleOn      = Color::FromARGB(0xFF34C759),
    .settingsToggleKnob    = Color::FromARGB(0xFFFFFFFF),

    .text      = Color::FromARGB(0xFFEDEDEF),
    .textMuted = Color::FromARGB(0x99EDEDEF),

    // ANSI 16: terminal "Pro" scheme, slightly desaturated for dark bg.
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

// Light palette (for future "Light" theme). Currently unused; kept here
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

    .captionMenuFill         = Color::FromARGB(0xD0FFFFFF),
    .captionMenuRowHover     = Color::FromARGB(0x14000000),
    .captionMenuText         = Color::FromARGB(0xFF1A1A1C),
    .captionMenuTextMuted    = Color::FromARGB(0x991A1A1C),

    .captionTitle      = Color::FromARGB(0xCC1A1A1C),
    .captionTitleMuted = Color::FromARGB(0x661A1A1C),
    .tabStripBg        = Color::FromARGB(0x00000000),
    .tabStripDivider   = Color::FromARGB(0x00000000),

    .alertScrim       = Color::FromARGB(0x66000000),
    .alertPanelFill   = Color::FromARGB(0xDDFFFFFF),
    .alertTitle       = Color::FromARGB(0xFF1A1A1C),
    .alertMessage     = Color::FromARGB(0xCC1A1A1C),
    .alertButtonFill  = Color::FromARGB(0xFF007AFF),
    .alertButtonHover = Color::FromARGB(0x14000000),
    .alertButtonText  = Color::FromARGB(0xFFFFFFFF),

    .settingsSidebarFill   = Color::FromARGB(0xA0FFFFFF),
    .settingsSidebarBorder = Color::FromARGB(0x22000000),
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

    .settingsCardFill      = Color::FromARGB(0xC2FFFFFF),
    .settingsCardBorder    = Color::FromARGB(0x1F000000),
    .settingsCardSeparator = Color::FromARGB(0x1A000000),
    .settingsSectionHeader = Color::FromARGB(0x991A1A1C),
    .settingsRowLabel      = Color::FromARGB(0xFF1A1A1C),
    .settingsRowValue      = Color::FromARGB(0x991A1A1C),
    .settingsRowChevron    = Color::FromARGB(0x801A1A1C),
    .settingsRowFooter     = Color::FromARGB(0x801A1A1C),

    .settingsToggleOff     = Color::FromARGB(0xFFD1D1D6),
    .settingsToggleOn      = Color::FromARGB(0xFF34C759),
    .settingsToggleKnob    = Color::FromARGB(0xFFFFFFFF),

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

}  // namespace vrtx::theme
