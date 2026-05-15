// Liquid Glass backdrop, level 1: real-time acrylic blur of whatever is
// behind the window. Layered on top, the renderer paints a tint and (later)
// noise to match the macOS Tahoe frosted-glass look.
//
// Implementation uses the undocumented but stable `SetWindowCompositionAttribute`
// + ACCENT_ENABLE_ACRYLICBLURBEHIND, which works identically on Windows 10
// build 17134+ and Windows 11. We deliberately do NOT use Mica/SystemBackdrop
// because (a) those are Win11-only, (b) they ignore window region, and
// (c) they bake in their own tint that fights ours.

#pragma once

#include "pch.h"

namespace mactw::window {

// Apply (or refresh) acrylic blur on the window. Color is the additional tint
// the OS applies inside the blur — we pass a near-zero-alpha value because we
// draw our own theme tint on top via D2D for full color control.
//
// Safe to call multiple times. On systems where the API isn't available (very
// old Win10) the call no-ops and returns false.
bool ApplyAcrylicBackdrop(HWND hwnd, uint32_t accentColorABGR = 0x01000000u);

// Remove any composition effect from the window.
void RemoveBackdrop(HWND hwnd);

}  // namespace mactw::window
