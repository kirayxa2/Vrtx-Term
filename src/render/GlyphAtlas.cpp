#include "render/GlyphAtlas.h"

#include "terminal/TermBuffer.h"   // for attr:: constants

#include <cstring>
#include <algorithm>

namespace vrtx::render {

namespace {

// Pad each glyph slot by this many texels on each side so bilinear sampling
// never bleeds into a neighbour's texels.
constexpr int kPad = 1;

}  // namespace

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void GlyphAtlas::Initialize(ID3D11Device*        device,
                             ID3D11DeviceContext* ctx,
                             IDWriteFactory*      dwrite,
                             IDWriteTextFormat*   fmtRegular,
                             IDWriteTextFormat*   fmtBold,
                             IDWriteTextFormat*   fmtItalic,
                             IDWriteTextFormat*   fmtBoldItalic,
                             float cellWPx,
                             float cellHPx) {
    dev_ = device;
    ctx_ = ctx;
    (void)dwrite; // not stored; GDI rasterisation doesn't need it

    fmts_[0] = fmtRegular;
    fmts_[1] = fmtBold;
    fmts_[2] = fmtItalic;
    fmts_[3] = fmtBoldItalic;
    cellW_         = std::max(1.0f, cellWPx);
    cellH_         = std::max(1.0f, cellHPx);
    font_size_px_  = cellHPx * 0.625f;  // ~font size from line height

    // ---- Atlas texture (A8_UNORM, dynamic upload via UpdateSubresource) ---
    D3D11_TEXTURE2D_DESC td{};
    td.Width            = kAtlasSize;
    td.Height           = kAtlasSize;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    // Zero-initialise so unused regions are transparent.
    std::vector<uint8_t> zero(kAtlasSize * kAtlasSize, 0u);
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem          = zero.data();
    sd.SysMemPitch      = kAtlasSize;

    HRESULT hr = dev_->CreateTexture2D(&td, &sd, tex_.GetAddressOf());
    if (FAILED(hr)) return;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format              = DXGI_FORMAT_A8_UNORM;
    srvd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    dev_->CreateShaderResourceView(tex_.Get(), &srvd, srv_.GetAddressOf());

    // Initialise shelf packer with one empty shelf at y=0.
    shelves_.push_back({0, 0, 0});
}

// ---------------------------------------------------------------------------
// OnMetricsChanged
// ---------------------------------------------------------------------------

void GlyphAtlas::OnMetricsChanged(IDWriteTextFormat* fmtRegular,
                                   IDWriteTextFormat* fmtBold,
                                   IDWriteTextFormat* fmtItalic,
                                   IDWriteTextFormat* fmtBoldItalic,
                                   float cellWPx,
                                   float cellHPx) {
    fmts_[0] = fmtRegular;
    fmts_[1] = fmtBold;
    fmts_[2] = fmtItalic;
    fmts_[3] = fmtBoldItalic;
    cellW_        = std::max(1.0f, cellWPx);
    cellH_        = std::max(1.0f, cellHPx);
    font_size_px_ = cellHPx * 0.625f;

    // All cached glyphs are the wrong size now - flush.
    Flush();
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------

void GlyphAtlas::Flush() {
    cache_.clear();
    shelves_.clear();
    shelves_.push_back({0, 0, 0});

    // Clear the atlas texture to transparent.
    if (tex_ && ctx_) {
        D3D11_BOX box{};
        box.right  = kAtlasSize;
        box.bottom = kAtlasSize;
        box.back   = 1;
        std::vector<uint8_t> zero(kAtlasSize * kAtlasSize, 0u);
        ctx_->UpdateSubresource(tex_.Get(), 0, nullptr,
                                zero.data(), kAtlasSize, 0);
    }
}

// ---------------------------------------------------------------------------
// AllocSlot  (shelf packer)
// ---------------------------------------------------------------------------

bool GlyphAtlas::AllocSlot(uint16_t w, uint16_t h,
                            uint16_t& outX, uint16_t& outY) {
    // Try to fit on an existing shelf.
    for (auto& shelf : shelves_) {
        if (h <= shelf.h || shelf.h == 0) {
            if (shelf.cursor + w <= static_cast<uint16_t>(kAtlasSize)) {
                outX = shelf.cursor;
                outY = shelf.y;
                shelf.cursor += w;
                shelf.h = std::max(shelf.h, h);
                return true;
            }
        }
    }

    // Open a new shelf below the last one.
    const uint16_t lastY = shelves_.empty()
        ? 0
        : static_cast<uint16_t>(shelves_.back().y + shelves_.back().h);

    if (lastY + h > static_cast<uint16_t>(kAtlasSize)) {
        return false;  // Atlas full.
    }

    shelves_.push_back({lastY, h, w});
    outX = 0;
    outY = lastY;
    return true;
}

// ---------------------------------------------------------------------------
// UploadBits
// ---------------------------------------------------------------------------

void GlyphAtlas::UploadBits(uint16_t ax, uint16_t ay,
                             uint16_t w,  uint16_t h,
                             const uint8_t* alpha, uint32_t stride) {
    if (!tex_ || !ctx_ || w == 0 || h == 0) return;

    D3D11_BOX box{};
    box.left   = ax;
    box.top    = ay;
    box.right  = ax + w;
    box.bottom = ay + h;
    box.back   = 1;

    ctx_->UpdateSubresource(tex_.Get(), 0, &box, alpha, stride, 0);
}

// ---------------------------------------------------------------------------
// FmtFor
// ---------------------------------------------------------------------------

IDWriteTextFormat* GlyphAtlas::FmtFor(uint16_t attrs) const {
    using namespace terminal::attr;
    const bool bold   = (attrs & kBold)   != 0;
    const bool italic = (attrs & kItalic) != 0;
    if (bold && italic) return fmts_[3];
    if (bold)           return fmts_[1];
    if (italic)         return fmts_[2];
    return fmts_[0];
}

// ---------------------------------------------------------------------------
// GetOrRasterize
// ---------------------------------------------------------------------------

bool GlyphAtlas::GetOrRasterize(const GlyphKey& key, GlyphSlot& outSlot) {
    // Fast path: already cached.
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        outSlot = it->second;
        return true;
    }

    IDWriteTextFormat* fmt = FmtFor(key.attrs);
    if (!fmt) return false;

    // Build UTF-16 representation of the codepoint.
    wchar_t wch[3]{};
    int     wchLen = 0;
    char32_t cp = key.codepoint;
    if (cp <= 0xFFFF) {
        wch[0]  = static_cast<wchar_t>(cp);
        wchLen  = 1;
    } else {
        cp -= 0x10000;
        wch[0]  = static_cast<wchar_t>(0xD800 + (cp >> 10));
        wch[1]  = static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
        wchLen  = 2;
    }

    // We rasterise at cell dimensions; add kPad on each side.
    const auto slotW = static_cast<uint16_t>(
        std::min<int>(static_cast<int>(std::ceil(cellW_)) + 2 * kPad,
                      kAtlasSize));
    const auto slotH = static_cast<uint16_t>(
        std::min<int>(static_cast<int>(std::ceil(cellH_)) + 2 * kPad,
                      kAtlasSize));

    uint16_t ax{}, ay{};
    if (!AllocSlot(slotW, slotH, ax, ay)) {
        return false;  // Atlas full - caller calls Flush() and retries.
    }

    // ---- Rasterise via GDI TextOut on a temporary DC -----------------------
    //
    // We create a temporary compatible DC + DIB, draw white text on black,
    // then extract the red channel as alpha coverage for the A8 atlas.

    const int rtW = static_cast<int>(std::ceil(cellW_)) + 2 * kPad;
    const int rtH = static_cast<int>(std::ceil(cellH_)) + 2 * kPad;

    // Retrieve the GDI font handle from the DWrite layout.
    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem    = ::CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = rtW;
    bi.bmiHeader.biHeight      = -rtH;  // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HBITMAP hBmp = ::CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    HBITMAP hOld = static_cast<HBITMAP>(::SelectObject(hdcMem, hBmp));

    // Black background.
    ::SetBkColor(hdcMem, RGB(0, 0, 0));
    ::SetTextColor(hdcMem, RGB(255, 255, 255));
    RECT rcFill{0, 0, rtW, rtH};
    ::FillRect(hdcMem, &rcFill, static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));

    // Get LOGFONT from the DWrite format and select it into the DC.
    LOGFONTW lf{};
    lf.lfHeight         = -static_cast<LONG>(std::round(font_size_px_));
    lf.lfWeight         = (key.attrs & terminal::attr::kBold) ? FW_BOLD : FW_NORMAL;
    lf.lfItalic         = (key.attrs & terminal::attr::kItalic) ? TRUE : FALSE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_TT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    {
        // Copy the family name from the DWrite format.
        wchar_t family[64] = {};
        fmt->GetFontFamilyName(family, 64);
        std::wcsncpy(lf.lfFaceName, family, LF_FACESIZE - 1);
    }
    HFONT hFont = ::CreateFontIndirectW(&lf);
    HFONT hOldFont = static_cast<HFONT>(::SelectObject(hdcMem, hFont));

    ::SetBkMode(hdcMem, TRANSPARENT);

    // Draw the glyph.
    if (wchLen == 1) {
        ::TextOutW(hdcMem, kPad, kPad, wch, 1);
    } else {
        ::TextOutW(hdcMem, kPad, kPad, wch, 2);
    }

    ::SelectObject(hdcMem, hOldFont);
    ::DeleteObject(hFont);

    // Read DIB bits.
    const int bmpW = rtW;
    const int bmpH = rtH;
    const auto* pixels = static_cast<const uint32_t*>(dibBits);

    // Extract alpha and upload.
    const int copyW = std::min(static_cast<int>(slotW), bmpW);
    const int copyH = std::min(static_cast<int>(slotH), bmpH);
    std::vector<uint8_t> alpha(static_cast<size_t>(copyW) * copyH);
    for (int y = 0; y < copyH; ++y) {
        for (int x = 0; x < copyW; ++x) {
            const uint32_t px = pixels[y * bmpW + x];
            alpha[y * copyW + x] = static_cast<uint8_t>((px >> 16) & 0xFF);
        }
    }

    // Cleanup GDI resources.
    ::SelectObject(hdcMem, hOld);
    ::DeleteObject(hBmp);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(nullptr, hdcScreen);

    UploadBits(ax, ay,
               static_cast<uint16_t>(copyW),
               static_cast<uint16_t>(copyH),
               alpha.data(), static_cast<uint32_t>(copyW));

    GlyphSlot slot{};
    slot.x        = ax;
    slot.y        = ay;
    slot.w        = static_cast<uint16_t>(copyW);
    slot.h        = static_cast<uint16_t>(copyH);
    slot.bearingX = static_cast<float>(kPad);
    slot.bearingY = static_cast<float>(kPad);
    slot.valid    = true;

    cache_.emplace(key, slot);
    outSlot = slot;
    return true;
}

}  // namespace vrtx::render
