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
#include "ui/TrafficLights.h"

namespace mactw::render {

class Renderer {
public:
    void Initialize(HWND hwnd);

    // Resize the swap chain after WM_SIZE. Width/height are physical pixels.
    void Resize(UINT widthPx, UINT heightPx);

    // Update DPI scale; call before Render() after WM_DPICHANGED.
    void SetDpi(UINT dpi) { dpi_ = dpi; }

    // Paint one frame.
    void Render(bool windowActive, ui::TrafficLights& trafficLights);

    UINT          Dpi()        const { return dpi_; }
    UINT          WidthPx()    const { return width_px_; }
    UINT          HeightPx()   const { return height_px_; }
    ID2D1Factory* D2DFactory() const { return d2d_factory_.Get(); }

private:
    void EnsureSwapChain(HWND hwnd);
    void RecreateBackBufferTarget();

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

    // DirectWrite
    ComPtr<IDWriteFactory>       dwrite_factory_;

    // DirectComposition
    ComPtr<IDCompositionDevice>  dcomp_device_;
    ComPtr<IDCompositionTarget>  dcomp_target_;
    ComPtr<IDCompositionVisual>  dcomp_visual_;
};

}  // namespace mactw::render
