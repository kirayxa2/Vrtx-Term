// DirectComposition-based renderer.
//
// We render via DirectComposition rather than a plain HWND swap chain. The
// crucial advantage is that DComp swap chains do not require a redirection
// surface, so we create the window with WS_EX_NOREDIRECTIONBITMAP and DWM
// has *no* place to paint its own frame, caption strip, or border. Whatever
// pixels we put in the DComp visual *is* the window. Transparency is
// per-pixel, anti-aliasing is handled by the compositor, and the squircle
// outline materialises automatically from the alpha channel.
//
// Pipeline:
//
//     D3D11Device
//          |
//          v
//     IDXGISwapChain (CreateSwapChainForComposition, premul alpha)
//          |
//          +--- D2D bitmap target -> rendered each frame by Renderer::Render
//          |
//          v
//     IDCompositionVisual.SetContent(swapchain)
//          |
//          v
//     IDCompositionTarget for the HWND
//          |
//          v
//     IDCompositionDevice.Commit()  -> compositor displays the swap chain.

#pragma once

#include "pch.h"
#include "render/TerminalView.h"
#include "terminal/TerminalSession.h"
#include "ui/AppAlert.h"
#include "ui/CaptionButton.h"
#include "ui/CaptionMenu.h"
#include "ui/SettingsView.h"
#include "ui/TabBar.h"
#include "ui/TrafficLights.h"

namespace vrtx::render {

class Renderer {
public:
    void Initialize(HWND hwnd);

    // Resize the swap chain after WM_SIZE. Width/height are physical pixels.
    void Resize(UINT widthPx, UINT heightPx);

    // Update DPI scale; call before Render() after WM_DPICHANGED.
    void SetDpi(UINT dpi);

    // Optional terminal session. Renderer does not own it.
    void SetSession(terminal::TerminalSession* session) { session_ = session; }

    // Compute the (cols, rows) that fit inside the squircle's content area
    // (i.e. squircle minus caption strip minus terminal padding) at the
    // current backbuffer size + DPI. Used by the window when sizing the pty.
    void GridForCurrentSize(int& cols, int& rows) const;

    // Cell metrics in physical pixels at the current DPI. Used by hit
    // testing in BorderlessWindow to map pointer coords -> grid cells.
    float CellWidthPx()  const { return terminal_view_.CellWidthPx();  }
    float CellHeightPx() const { return terminal_view_.CellHeightPx(); }
    float TerminalPaddingXPx() const { return terminal_view_.PaddingXPx(); }
    float TerminalPaddingYPx() const { return terminal_view_.PaddingYPx(); }

    // Paint one frame.
    void Render(bool windowActive,
                bool cursorVisible,
                ui::TabBar&        tabBar,
                ui::TrafficLights& trafficLights,
                ui::CaptionButton& captionButton,
                ui::CaptionMenu&   captionMenu,
                ui::AppAlert&      appAlert,
                ui::SettingsView&  settings);

    UINT          Dpi()        const { return dpi_; }
    UINT          WidthPx()    const { return width_px_; }
    UINT          HeightPx()   const { return height_px_; }
    ID2D1Factory* D2DFactory() const { return d2d_factory_.Get(); }

private:
    void EnsureSwapChain(HWND hwnd);
    void RecreateBackBufferTarget();
    void EnsureNoiseBrush();

    // Build the centred caption title ("user — bash — 80×24") into a
    // cached IDWriteTextLayout. Re-built lazily when the inputs change.
    void EnsureTitleLayout(int cols, int rows);

    HWND hwnd_{nullptr};

    UINT dpi_       {96};
    UINT width_px_  {0};
    UINT height_px_ {0};

    // D3D
    ComPtr<ID3D11Device>        d3d_device_;
    ComPtr<ID3D11DeviceContext> d3d_context_;

    // DXGI swap chain: created via CreateSwapChainForComposition (no HWND).
    ComPtr<IDXGISwapChain1>     swap_chain_;

    // Direct2D
    ComPtr<ID2D1Factory1>        d2d_factory_;
    ComPtr<ID2D1Device>          d2d_device_;
    ComPtr<ID2D1DeviceContext>   d2d_dc_;
    ComPtr<ID2D1Bitmap1>         d2d_back_buffer_;
    ComPtr<ID2D1SolidColorBrush> brush_;

    // Frosted-glass noise overlay. A 128x128 deterministic-noise
    // bitmap tiled by a wrap-mode bitmap brush. Drawn twice per
    // frame at very low opacity:
    //
    //   * Once on top of the squircle fill, so the base window
    //     surface reads as frosted glass rather than flat tint.
    //   * Once on top of the chrome (sidebar pill, cards, caption
    //     button, menu, alert), so those translucent panes carry
    //     the same grain as the surface they sit on - the eye reads
    //     them as the same physical material.
    //
    // The bitmap is built once during Initialize(); the brush is
    // reused across frames so there is no per-frame allocation.
    ComPtr<ID2D1Bitmap>      noise_bitmap_;
    ComPtr<ID2D1BitmapBrush> noise_brush_;

    // DirectWrite
    ComPtr<IDWriteFactory>       dwrite_factory_;

    // DirectComposition
    ComPtr<IDCompositionDevice>  dcomp_device_;
    ComPtr<IDCompositionTarget>  dcomp_target_;
    ComPtr<IDCompositionVisual>  dcomp_visual_;

    // Terminal grid renderer (owns DWrite text formats).
    TerminalView terminal_view_;
    terminal::TerminalSession* session_{nullptr};

    // Cached caption title layout. Recomputed when any of (username,
    // shell exe basename, cols, rows, dpi) changes.
    ComPtr<IDWriteTextFormat> title_fmt_;
    ComPtr<IDWriteTextLayout> title_layout_;
    std::wstring              cached_username_;     // resolved once at Initialize()
    std::wstring              cached_shell_;        // basename of session_->ShellPath()
    int                       cached_title_cols_{0};
    int                       cached_title_rows_{0};
    UINT                      cached_title_dpi_{0};
};

}  // namespace vrtx::render
