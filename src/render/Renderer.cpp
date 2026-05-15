#include "render/Renderer.h"

#include "theme/TahoeTheme.h"
#include "window/SquircleGeometry.h"

namespace mactw::render {

namespace {

inline D2D1::ColorF ToD2D(theme::Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

}  // namespace

void Renderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;

    // ---- D3D11 device ------------------------------------------------------
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
    };

    HRESULT hr = ::D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        d3d_device_.GetAddressOf(), nullptr, d3d_context_.GetAddressOf());

    if (FAILED(hr)) {
        hr = ::D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(), nullptr, d3d_context_.GetAddressOf());
        ThrowIfFailed(hr, "D3D11CreateDevice (WARP fallback)");
    }

    // ---- D2D factory + device + context -----------------------------------
    D2D1_FACTORY_OPTIONS factoryOptions{};
    ThrowIfFailed(::D2D1CreateFactory(
                      D2D1_FACTORY_TYPE_SINGLE_THREADED,
                      __uuidof(ID2D1Factory1), &factoryOptions,
                      reinterpret_cast<void**>(d2d_factory_.GetAddressOf())),
                  "D2D1CreateFactory");

    ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(d3d_device_.As(&dxgiDevice), "QI IDXGIDevice");
    ThrowIfFailed(d2d_factory_->CreateDevice(dxgiDevice.Get(),
                                             d2d_device_.GetAddressOf()),
                  "ID2D1Factory1::CreateDevice");
    ThrowIfFailed(d2d_device_->CreateDeviceContext(
                      D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                      d2d_dc_.GetAddressOf()),
                  "ID2D1Device::CreateDeviceContext");

    // ---- DirectWrite -------------------------------------------------------
    ThrowIfFailed(::DWriteCreateFactory(
                      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                      reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf())),
                  "DWriteCreateFactory");

    // ---- DirectComposition device ----------------------------------------
    //
    // The DComp device is the root object for all DComp APIs. We bind it to
    // our DXGI device so the compositor can sample our swap chain directly.
    ThrowIfFailed(::DCompositionCreateDevice(
                      dxgiDevice.Get(), __uuidof(IDCompositionDevice),
                      reinterpret_cast<void**>(dcomp_device_.GetAddressOf())),
                  "DCompositionCreateDevice");

    // The target ties a DComp visual tree to a specific HWND. Since the
    // window is created with WS_EX_NOREDIRECTIONBITMAP, this visual *is* the
    // window's pixels - DWM has no separate canvas to paint over.
    ThrowIfFailed(dcomp_device_->CreateTargetForHwnd(
                      hwnd, /*topmost=*/TRUE, dcomp_target_.GetAddressOf()),
                  "IDCompositionDevice::CreateTargetForHwnd");

    ThrowIfFailed(dcomp_device_->CreateVisual(dcomp_visual_.GetAddressOf()),
                  "IDCompositionDevice::CreateVisual");

    EnsureSwapChain(hwnd);

    // Initialise the terminal grid renderer with our DWrite factory.
    terminal_view_.Initialize(dwrite_factory_.Get(), dpi_);

    ThrowIfFailed(dcomp_target_->SetRoot(dcomp_visual_.Get()),
                  "IDCompositionTarget::SetRoot");
    ThrowIfFailed(dcomp_device_->Commit(), "IDCompositionDevice::Commit");
}

void Renderer::SetDpi(UINT dpi) {
    if (dpi == dpi_) return;
    dpi_ = dpi;
    terminal_view_.OnDpiChanged(dpi);
}

void Renderer::GridForCurrentSize(int& cols, int& rows) const {
    const float marginPx  = theme::ToPx(theme::kShadowMargin,  dpi_);
    const float captionPx = theme::ToPx(theme::kCaptionHeight, dpi_);
    const float swW = std::max(1.0f, static_cast<float>(width_px_)  - 2.0f * marginPx);
    const float swH = std::max(1.0f, static_cast<float>(height_px_) - 2.0f * marginPx);
    const float contentW = swW;
    const float contentH = std::max(1.0f, swH - captionPx);
    terminal_view_.GridForContent(contentW, contentH, cols, rows);
}

void Renderer::EnsureSwapChain(HWND hwnd) {
    ComPtr<IDXGIDevice>   dxgiDevice;
    ComPtr<IDXGIAdapter>  dxgiAdapter;
    ComPtr<IDXGIFactory2> dxgiFactory;

    ThrowIfFailed(d3d_device_.As(&dxgiDevice), "QI IDXGIDevice");
    ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()),
                  "IDXGIDevice::GetAdapter");
    ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())),
                  "IDXGIAdapter::GetParent IDXGIFactory2");

    RECT rc{};
    ::GetClientRect(hwnd, &rc);
    width_px_  = std::max<UINT>(1, rc.right  - rc.left);
    height_px_ = std::max<UINT>(1, rc.bottom - rc.top);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width            = width_px_;
    desc.Height           = height_px_;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = 2;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // Composition swap chains *do* support per-pixel alpha. Premultiplied
    // alpha lets transparent pixels in the squircle's outer area pass
    // through and reveal whatever is behind the window.
    desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;

    ThrowIfFailed(dxgiFactory->CreateSwapChainForComposition(
                      d3d_device_.Get(), &desc, nullptr,
                      swap_chain_.GetAddressOf()),
                  "IDXGIFactory2::CreateSwapChainForComposition");

    ThrowIfFailed(dcomp_visual_->SetContent(swap_chain_.Get()),
                  "IDCompositionVisual::SetContent");

    RecreateBackBufferTarget();
}

void Renderer::RecreateBackBufferTarget() {
    d2d_back_buffer_.Reset();

    ComPtr<IDXGISurface> surface;
    ThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())),
                  "IDXGISwapChain::GetBuffer");

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ThrowIfFailed(d2d_dc_->CreateBitmapFromDxgiSurface(
                      surface.Get(), &props,
                      d2d_back_buffer_.GetAddressOf()),
                  "D2D1DeviceContext::CreateBitmapFromDxgiSurface");

    d2d_dc_->SetTarget(d2d_back_buffer_.Get());

    if (!brush_) {
        ThrowIfFailed(d2d_dc_->CreateSolidColorBrush(
                          D2D1::ColorF(D2D1::ColorF::White),
                          brush_.GetAddressOf()),
                      "ID2D1DeviceContext::CreateSolidColorBrush");
    }
}

void Renderer::Resize(UINT widthPx, UINT heightPx) {
    width_px_  = std::max<UINT>(1, widthPx);
    height_px_ = std::max<UINT>(1, heightPx);

    if (!swap_chain_) return;

    d2d_dc_->SetTarget(nullptr);
    d2d_back_buffer_.Reset();

    ThrowIfFailed(swap_chain_->ResizeBuffers(0, width_px_, height_px_,
                                             DXGI_FORMAT_UNKNOWN, 0),
                  "IDXGISwapChain::ResizeBuffers");

    RecreateBackBufferTarget();
}

void Renderer::Render(bool windowActive, ui::TrafficLights& trafficLights) {
    if (!d2d_dc_ || !swap_chain_) return;

    const auto& pal      = theme::ActivePalette();
    const float radius   = theme::ToPx(theme::kWindowCornerRadius, dpi_);
    const float marginPx = theme::ToPx(theme::kShadowMargin,       dpi_);
    const float width    = static_cast<float>(width_px_);
    const float height   = static_cast<float>(height_px_);

    // The visible squircle sits inside the HWND inset by `marginPx` on every
    // side; the outer band is the room reserved for the soft drop shadow.
    const float swW = std::max(1.0f, width  - 2.0f * marginPx);
    const float swH = std::max(1.0f, height - 2.0f * marginPx);

    // Squircle path is built at (0, 0); we translate it into place each time
    // so the same geometry serves the shadow pass, the fill, the clip layer,
    // and the hairline outline.
    auto squircle = window::BuildSquirclePath(d2d_factory_.Get(),
                                              swW, swH,
                                              radius, theme::kSquircleSmoothing);

    d2d_dc_->BeginDraw();
    d2d_dc_->SetTransform(D2D1::Matrix3x2F::Identity());

    // Fully transparent canvas. Everything outside the squircle (including
    // the shadow band) stays at alpha 0 wherever the shadow doesn't reach,
    // so DComp lets the desktop show through.
    d2d_dc_->Clear(D2D1::ColorF(0, 0, 0, 0));

    // ---- Drop shadow pass -------------------------------------------------
    //
    // Mirrors the macOS Tahoe SVG `filter: drop-shadow(0 5pt 15pt rgba(0,0,0,0.30))`:
    //
    //   1. Record an opaque-shape command list that fills the squircle with
    //      pre-multiplied rgba(0,0,0, kShadowAlpha) at offset (margin,
    //      margin + kShadowOffsetY). Pre-baking the alpha into the brush
    //      saves us a separate D2D opacity effect after the blur.
    //   2. Feed the command list into a Gaussian blur with
    //      stdDeviation = kShadowBlurStd and BORDER_MODE_SOFT so the result
    //      fades to alpha 0 well inside `kShadowMargin`.
    //   3. DrawImage onto the back buffer FIRST, before the squircle fill,
    //      so the opaque squircle covers the inner half of the shadow on
    //      the overlap and only the soft outer halo remains visible.
    {
        ComPtr<ID2D1CommandList> cmdList;
        ThrowIfFailed(d2d_dc_->CreateCommandList(cmdList.GetAddressOf()),
                      "ID2D1DeviceContext::CreateCommandList");

        d2d_dc_->SetTarget(cmdList.Get());

        ComPtr<ID2D1SolidColorBrush> shadowBrush;
        ThrowIfFailed(d2d_dc_->CreateSolidColorBrush(
                          D2D1::ColorF(0.0f, 0.0f, 0.0f, theme::kShadowAlpha),
                          shadowBrush.GetAddressOf()),
                      "CreateSolidColorBrush(shadow)");

        const float shadowOffsetPx = theme::ToPx(theme::kShadowOffsetY, dpi_);
        d2d_dc_->SetTransform(D2D1::Matrix3x2F::Translation(
            marginPx, marginPx + shadowOffsetPx));
        d2d_dc_->FillGeometry(squircle.Get(), shadowBrush.Get());
        d2d_dc_->SetTransform(D2D1::Matrix3x2F::Identity());

        ThrowIfFailed(cmdList->Close(), "ID2D1CommandList::Close");

        // Switch back to the swap-chain bitmap before consuming the cmdList.
        d2d_dc_->SetTarget(d2d_back_buffer_.Get());
        d2d_dc_->SetTransform(D2D1::Matrix3x2F::Identity());

        ComPtr<ID2D1Effect> blur;
        ThrowIfFailed(d2d_dc_->CreateEffect(CLSID_D2D1GaussianBlur,
                                            blur.GetAddressOf()),
                      "CreateEffect(GaussianBlur)");
        blur->SetInput(0, cmdList.Get());
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,
                       theme::ToPx(theme::kShadowBlurStd, dpi_));
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE,
                       D2D1_BORDER_MODE_SOFT);

        d2d_dc_->DrawImage(blur.Get(), nullptr, nullptr,
                           D2D1_INTERPOLATION_MODE_LINEAR,
                           D2D1_COMPOSITE_MODE_SOURCE_OVER);
    }

    // ---- Squircle fill ----------------------------------------------------
    //
    // From here on every draw is in squircle-local coordinates by virtue of
    // the active (margin, margin) translation.
    d2d_dc_->SetTransform(D2D1::Matrix3x2F::Translation(marginPx, marginPx));
    brush_->SetColor(ToD2D(pal.windowTint));
    d2d_dc_->FillGeometry(squircle.Get(), brush_.Get());

    // ---- Push squircle clip for the rest of the UI -----------------------
    ComPtr<ID2D1Layer> layer;
    d2d_dc_->CreateLayer(nullptr, layer.GetAddressOf());
    d2d_dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), squircle.Get()),
                       layer.Get());

    // ---- Terminal grid ---------------------------------------------------
    //
    // Content area starts immediately below the caption strip and fills the
    // remainder of the squircle. TerminalView adds its own padding inside.
    if (session_) {
        const float captionPx = theme::ToPx(theme::kCaptionHeight, dpi_);
        D2D1_RECT_F contentRect{
            0.0f,
            captionPx,
            swW,
            swH,
        };
        terminal_view_.Draw(d2d_dc_.Get(), *session_, contentRect, windowActive);
    }

    // Traffic lights produce squircle-local coordinates already; the active
    // transform places them inside the window correctly.
    trafficLights.Render(d2d_dc_.Get(), brush_.Get(), d2d_factory_.Get(),
                         windowActive);

    d2d_dc_->PopLayer();

    // ---- Hairline outline ------------------------------------------------
    //
    // 1pt light rim around the squircle, drawn AFTER PopLayer so it is not
    // clipped by the squircle layer itself. We inset the geometry by half a
    // pixel so the stroke - which D2D centres on the path - is fully inside
    // the visible window pixels rather than half-cut by the alpha edge.
    {
        const float strokePx = std::max(1.0f,
                                        theme::ToPx(theme::kWindowBorderWidth, dpi_));
        const float inset = strokePx * 0.5f;

        auto outline = window::BuildSquirclePath(
            d2d_factory_.Get(),
            swW - 2.0f * inset,
            swH - 2.0f * inset,
            std::max(0.0f, radius - inset),
            theme::kSquircleSmoothing);

        d2d_dc_->SetTransform(D2D1::Matrix3x2F::Translation(
            marginPx + inset, marginPx + inset));
        brush_->SetColor(ToD2D(pal.windowBorder));
        d2d_dc_->DrawGeometry(outline.Get(), brush_.Get(), strokePx);
    }

    d2d_dc_->SetTransform(D2D1::Matrix3x2F::Identity());

    HRESULT hr = d2d_dc_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        d2d_dc_->SetTarget(nullptr);
        d2d_back_buffer_.Reset();
        RecreateBackBufferTarget();
        return;
    }
    ThrowIfFailed(hr, "ID2D1DeviceContext::EndDraw");

    DXGI_PRESENT_PARAMETERS pp{};
    swap_chain_->Present1(1, 0, &pp);

    // Tell the compositor a new frame is ready.
    dcomp_device_->Commit();
}

}  // namespace mactw::render
