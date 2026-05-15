// Direct2D / Direct3D11 / DirectComposition renderer.
//
// Pipeline:
//   D3D11Device + DXGI swap chain (FLIP, premultiplied alpha)
//                |
//                v
//     D2D1Device built from the DXGI device
//                |
//                v
//     D2D1DeviceContext with target = swap chain back buffer
//                |
//                v
//     Each frame: Begin -> draw squircle clipped fill -> traffic lights ->
//     content stub -> End -> Present.
//
// This class is intentionally NOT a render thread. Drawing happens on the UI
// thread, kicked off by WM_PAINT or by an explicit RequestPaint() call from
// the window after a state change.

#pragma once

#include "pch.h"
#include "ui/TrafficLights.h"

namespace mactw::render {

class Renderer {
public:
    void Initialize(HWND hwnd);

    // Resize swap chain after WM_SIZE. Width/height are physical pixels.
    void Resize(UINT widthPx, UINT heightPx);

    // Update DPI scale; call before Render() after WM_DPICHANGED.
    void SetDpi(UINT dpi) { dpi_ = dpi; }

    // Paint one frame. Caller has already updated logical state.
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

    // D3D / DXGI
    ComPtr<ID3D11Device>        d3d_device_;
    ComPtr<ID3D11DeviceContext> d3d_context_;
    ComPtr<IDXGISwapChain1>     swap_chain_;

    // Direct2D
    ComPtr<ID2D1Factory1>       d2d_factory_;
    ComPtr<ID2D1Device>         d2d_device_;
    ComPtr<ID2D1DeviceContext>  d2d_dc_;
    ComPtr<ID2D1Bitmap1>        d2d_back_buffer_;
    ComPtr<ID2D1SolidColorBrush> brush_;

    // DirectWrite (for caption text once we add a title; unused for MVP)
    ComPtr<IDWriteFactory>      dwrite_factory_;
};

}  // namespace mactw::render
