// Borderless top-level window with manual non-client logic.
//
// Strategy (well-trodden Win32 path, used by Microsoft Edge, VS Code, Spotify
// and others):
//
//   1. Register a normal class with WS_OVERLAPPEDWINDOW so we still get
//      proper resize/min/max behaviour from the window manager.
//   2. Intercept WM_NCCALCSIZE and return 0 with WVR_DEFAULT cleared, which
//      tells DWM "the entire window is client area" — i.e. no system caption,
//      no system border.
//   3. Intercept WM_NCHITTEST so we can:
//        - report HTCAPTION over the draggable strip,
//        - report HTLEFT / HTTOP / etc. on the resize border,
//        - report HTCLIENT for the traffic lights so we receive their clicks.
//   4. SetWindowRgn to a squircle polygon so acrylic + actual window pixels
//      are clipped to our shape. Refreshed on every resize and DPI change.
//
// We deliberately keep this class engine-agnostic: it owns the HWND, dispatches
// events to a Renderer + TrafficLights pair, but knows nothing about how those
// are implemented.

#pragma once

#include "pch.h"
#include "render/Renderer.h"
#include "ui/TrafficLights.h"

namespace mactw::window {

class BorderlessWindow {
public:
    BorderlessWindow();
    ~BorderlessWindow();

    BorderlessWindow(const BorderlessWindow&)            = delete;
    BorderlessWindow& operator=(const BorderlessWindow&) = delete;

    // Create the HWND and show it. Returns the HWND on success, throws on
    // failure.
    HWND Create(HINSTANCE hInstance, const wchar_t* title);

    HWND Hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);

    // Recompute squircle window region after size or DPI change.
    void UpdateWindowRegion();

    // WM_NCHITTEST helper: classify a screen-space point.
    LRESULT HitTest(POINT screenPt) const;

    // Apply the per-monitor DPI to renderer + traffic-lights.
    void OnDpiChanged(UINT newDpi, const RECT* suggested);

    HWND       hwnd_   {nullptr};
    HINSTANCE  hinst_  {nullptr};
    UINT       dpi_    {96};
    bool       active_ {true};

    render::Renderer  renderer_;
    ui::TrafficLights traffic_;
};

}  // namespace mactw::window
