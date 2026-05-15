// Borderless top-level window driven entirely by DirectComposition.
//
// Why this design works where the conventional "extend frame into client
// area" trick fails:
//
//   * The HWND is created with WS_EX_NOREDIRECTIONBITMAP. That means DWM
//     never allocates a redirection surface for it. There is literally no
//     pixel buffer the system could paint a frame onto.
//   * The Renderer creates a DirectComposition swap chain via
//     CreateSwapChainForComposition (no HWND), wraps it in a DComp visual,
//     and binds that visual to the HWND through IDCompositionTarget. From
//     this point on, the only pixels that show up where the window lives
//     are pixels we draw into the swap chain. DWM has no opinion to express.
//   * Squircle outline materialises automatically from alpha. We don't need
//     SetWindowRgn, which (a) clips to integer pixel polygons (visibly
//     jaggy at small radii) and (b) masks out hit testing as well as
//     visuals, breaking edge-resize.
//
// Resize and move are driven manually because WS_THICKFRAME is gone. We
// classify the cursor location in WM_NCHITTEST and on WM_NCLBUTTONDOWN we
// kick off a system SC_SIZE / SC_MOVE which gives us all the standard
// modifier behaviour (Aero Snap, Win+arrow tiling, etc.) without DWM ever
// drawing a frame.

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

    HWND Create(HINSTANCE hInstance, const wchar_t* title);
    HWND Hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);

    LRESULT HitTest(POINT screenPt) const;
    void OnDpiChanged(UINT newDpi, const RECT* suggested);

    HWND      hwnd_   {nullptr};
    HINSTANCE hinst_  {nullptr};
    UINT      dpi_    {96};
    bool      active_ {true};

    render::Renderer  renderer_;
    ui::TrafficLights traffic_;
};

}  // namespace mactw::window
