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

    // ---- D3D11 + DXGI ------------------------------------------------------
    UINT createFlags = 0;
#if !defined(NDEBUG)
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    createFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // required for D2D

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
    };

    HRESULT hr = ::D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        d3d_device_.GetAddressOf(), nullptr, d3d_context_.GetAddressOf());

    if (FAILED(hr)) {
        // Fall back to WARP if the GPU rejects us (e.g. no hardware acceleration).
        hr = ::D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createFlags & ~D3D11_CREATE_DEVICE_DEBUG, featureLevels,
            ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(), nullptr, d3d_context_.GetAddressOf());
        ThrowIfFailed(hr, "D3D11CreateDevice (WARP)");
    }

    // ---- D2D factory + device ---------------------------------------------
    D2D1_FACTORY_OPTIONS factoryOptions{};
#if !defined(NDEBUG)
    factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    ThrowIfFailed(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                      __uuidof(ID2D1Factory1), &factoryOptions,
                                      reinterpret_cast<void**>(d2d_factory_.GetAddressOf())),
                  "D2D1CreateFactory");

    ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(d3d_device_.As(&dxgiDevice), "QueryInterface IDXGIDevice");
    ThrowIfFailed(d2d_factory_->CreateDevice(dxgiDevice.Get(),
                                             d2d_device_.GetAddressOf()),
                  "ID2D1Factory1::CreateDevice");
    ThrowIfFailed(d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                   d2d_dc_.GetAddressOf()),
                  "ID2D1Device::CreateDeviceContext");

    // ---- DirectWrite -------------------------------------------------------
    ThrowIfFailed(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                        __uuidof(IDWriteFactory),
                                        reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf())),
                  "DWriteCreateFactory");

    EnsureSwapChain(hwnd);
}

void Renderer::EnsureSwapChain(HWND hwnd) {
    ComPtr<IDXGIDevice>  dxgiDevice;
    ComPtr<IDXGIAdapter> dxgiAdapter;
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
    desc.Width       = width_px_;
    desc.Height      = height_px_;
    desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;

    // Acrylic underneath us already provides the blur. We use FLIP_SEQUENTIAL
    // with premultiplied alpha so transparent pixels we draw show through.
    ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
                      d3d_device_.Get(), hwnd, &desc, nullptr, nullptr,
                      swap_chain_.GetAddressOf()),
                  "IDXGIFactory2::CreateSwapChainForHwnd");

    // Don't let DXGI intercept Alt+Enter — we own that interaction.
    dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    RecreateBackBufferTarget();
}

void Renderer::RecreateBackBufferTarget() {
    d2d_back_buffer_.Reset();

    ComPtr<IDXGISurface> surface;
    ThrowIfFailed(swap_chain_->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())),
                  "IDXGISwapChain::GetBuffer");

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ThrowIfFailed(d2d_dc_->CreateBitmapFromDxgiSurface(surface.Get(), &props,
                                                      d2d_back_buffer_.GetAddressOf()),
                  "D2D1DeviceContext::CreateBitmapFromDxgiSurface");

    d2d_dc_->SetTarget(d2d_back_buffer_.Get());

    if (!brush_) {
        ThrowIfFailed(d2d_dc_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                                    brush_.GetAddressOf()),
                      "ID2D1DeviceContext::CreateSolidColorBrush");
    }
}

void Renderer::Resize(UINT widthPx, UINT heightPx) {
    width_px_  = std::max<UINT>(1, widthPx);
    height_px_ = std::max<UINT>(1, heightPx);

    if (!swap_chain_) return;

    // Release any references to the old back buffer.
    d2d_dc_->SetTarget(nullptr);
    d2d_back_buffer_.Reset();

    ThrowIfFailed(swap_chain_->ResizeBuffers(0, width_px_, height_px_,
                                             DXGI_FORMAT_UNKNOWN, 0),
                  "IDXGISwapChain::ResizeBuffers");

    RecreateBackBufferTarget();
}

void Renderer::Render(bool windowActive, ui::TrafficLights& trafficLights) {
    if (!d2d_dc_ || !swap_chain_) return;

    const auto&  pal     = theme::ActivePalette();
    const float  radius  = theme::ToPx(theme::kRadiusTitlebarWindow, dpi_);
    const float  width   = static_cast<float>(width_px_);
    const float  height  = static_cast<float>(height_px_);

    auto squircle = window::BuildSquirclePath(d2d_factory_.Get(), width, height,
                                              radius, theme::kSquircleSmoothing);

    d2d_dc_->BeginDraw();
    d2d_dc_->SetTransform(D2D1::Matrix3x2F::Identity());

    // Fully transparent canvas: the window itself is layered atop acrylic.
    d2d_dc_->Clear(D2D1::ColorF(0, 0, 0, 0));

    // Push squircle clip via a layer geometry, so everything we draw next
    // (tint, content, traffic lights) is automatically rounded.
    ComPtr<ID2D1Layer> layer;
    d2d_dc_->CreateLayer(nullptr, layer.GetAddressOf());
    d2d_dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), squircle.Get()),
                       layer.Get());

    // ---- Layer 1: tint -----------------------------------------------------
    brush_->SetColor(ToD2D(pal.windowTint));
    d2d_dc_->FillRectangle(D2D1::RectF(0, 0, width, height), brush_.Get());

    // ---- Layer 2: caption + content placeholder ---------------------------
    const float captionHpx = theme::ToPx(theme::kCaptionHeight, dpi_);

    // Subtle separator hairline below the caption strip.
    {
        theme::Color sep = pal.windowBorder;
        sep.a *= 0.6f;
        brush_->SetColor(ToD2D(sep));
        d2d_dc_->DrawLine(D2D1::Point2F(0,     captionHpx + 0.5f),
                          D2D1::Point2F(width, captionHpx + 0.5f),
                          brush_.Get(), 1.0f);
    }

    // Content placeholder text — gives the window something visible until the
    // terminal core lands.
    {
        ComPtr<IDWriteTextFormat> fmt;
        const wchar_t* fontFamilies[] = {L"Cascadia Code", L"Consolas"};
        for (const wchar_t* family : fontFamilies) {
            if (SUCCEEDED(dwrite_factory_->CreateTextFormat(
                    family, nullptr, DWRITE_FONT_WEIGHT_REGULAR,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    14.0f * dpi_ / 96.0f, L"en-us", fmt.GetAddressOf()))) {
                break;
            }
        }
        if (fmt) {
            fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* placeholder =
                L"Terminal coming soon \u2014 squircle radius 16pt";
            brush_->SetColor(ToD2D(pal.textMuted));
            d2d_dc_->DrawText(placeholder,
                              static_cast<UINT32>(wcslen(placeholder)),
                              fmt.Get(),
                              D2D1::RectF(0, captionHpx, width, height),
                              brush_.Get());
        }
    }

    // ---- Layer 3: traffic lights ------------------------------------------
    trafficLights.Render(d2d_dc_.Get(), brush_.Get(), d2d_factory_.Get(), windowActive);

    // ---- Layer 4: 1px hairline border, drawn last so it sits on top -------
    brush_->SetColor(ToD2D(pal.windowBorder));
    d2d_dc_->DrawGeometry(squircle.Get(), brush_.Get(), 1.0f);

    d2d_dc_->PopLayer();

    HRESULT hr = d2d_dc_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        // Device lost: rebuild and skip this frame.
        d2d_dc_->SetTarget(nullptr);
        d2d_back_buffer_.Reset();
        RecreateBackBufferTarget();
        return;
    }
    ThrowIfFailed(hr, "ID2D1DeviceContext::EndDraw");

    DXGI_PRESENT_PARAMETERS pp{};
    swap_chain_->Present1(1, 0, &pp);
}

}  // namespace mactw::render
