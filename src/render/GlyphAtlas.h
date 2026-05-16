// GlyphAtlas - GPU-resident glyph cache for the terminal renderer.
//
// Strategy
// --------
// A single DXGI_FORMAT_A8_UNORM texture (kAtlasSize x kAtlasSize) holds
// every distinct glyph the terminal has rendered since the last flush.
// Glyphs are packed by a simple shelf algorithm.  Each slot is rasterised
// once via IDWriteBitmapRenderTarget (GDI-compatible, CPU) and uploaded
// with UpdateSubresource.
//
// Compared with the DrawTextW path
// ---------------------------------
//   DrawTextW fires one DWrite layout + one D2D call per color-run.
//   At 200x60 with mixed colors that is hundreds of calls per frame.
//   The atlas path collapses everything into a single instanced quad
//   draw, cutting GPU time to microseconds even at 4K.
//
// Eviction
// --------
// When the atlas is full we call Flush() which wipes all slots and
// restarts the packer.  A full flush is rare: the 2048x2048 atlas
// holds ~4000 average-size glyphs, far more than any real session needs.

#pragma once

#include "pch.h"
#include <unordered_map>
#include <vector>

namespace vrtx::render {

// ---- GlyphKey -------------------------------------------------------------

struct GlyphKey {
    char32_t codepoint{0};
    uint16_t attrs{0};   // kBold | kItalic only
    uint16_t _pad{0};

    bool operator==(const GlyphKey& o) const noexcept {
        return codepoint == o.codepoint && attrs == o.attrs;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const noexcept {
        size_t h = 14695981039346656037ull;
        h ^= static_cast<size_t>(k.codepoint); h *= 1099511628211ull;
        h ^= static_cast<size_t>(k.attrs);     h *= 1099511628211ull;
        return h;
    }
};

// ---- GlyphSlot ------------------------------------------------------------

// Where a glyph lives in the atlas (texel coordinates).
struct GlyphSlot {
    uint16_t x{0}, y{0}, w{0}, h{0};
    // Offset from the cell top-left to the glyph black-box top-left (px).
    float bearingX{0.0f};
    float bearingY{0.0f};
    bool  valid{false};
};

// ---- GlyphInstance --------------------------------------------------------

// One entry in the per-frame instanced draw list.
// Matches the cbuffer layout in the vertex shader.
#pragma pack(push, 4)
struct GlyphInstance {
    float destX, destY;       // cell top-left in squircle-local px
    float srcU0, srcV0;       // atlas UV top-left  [0..1]
    float srcU1, srcV1;       // atlas UV bot-right [0..1]
    float offX,  offY;        // bearing offset inside the cell
    float r, g, b, a;         // foreground colour
};
#pragma pack(pop)

// ---- GlyphAtlas -----------------------------------------------------------

class GlyphAtlas {
public:
    static constexpr int kAtlasSize = 2048;

    GlyphAtlas()  = default;
    ~GlyphAtlas() = default;

    GlyphAtlas(const GlyphAtlas&)            = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    // One-time initialisation.  Pass the four text formats (caller owns them).
    void Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext* ctx,
                    IDWriteFactory*      dwrite,
                    IDWriteTextFormat*   fmtRegular,
                    IDWriteTextFormat*   fmtBold,
                    IDWriteTextFormat*   fmtItalic,
                    IDWriteTextFormat*   fmtBoldItalic,
                    float cellWPx,
                    float cellHPx);

    // Call when DPI or font size changes.
    void OnMetricsChanged(IDWriteTextFormat* fmtRegular,
                          IDWriteTextFormat* fmtBold,
                          IDWriteTextFormat* fmtItalic,
                          IDWriteTextFormat* fmtBoldItalic,
                          float cellWPx,
                          float cellHPx);

    // Return the atlas slot for key, rasterising it first if necessary.
    // Returns false when the atlas is full; caller must Flush() and retry.
    bool GetOrRasterize(const GlyphKey& key, GlyphSlot& outSlot);

    // Evict all glyphs and reset packer.
    void Flush();

    ID3D11ShaderResourceView* SRV()        const { return srv_.Get(); }
    float                     AtlasSizeF() const { return static_cast<float>(kAtlasSize); }

private:
    struct Shelf { uint16_t y, h, cursor; };

    bool AllocSlot(uint16_t w, uint16_t h, uint16_t& outX, uint16_t& outY);
    void UploadBits(uint16_t ax, uint16_t ay,
                    uint16_t w,  uint16_t h,
                    const uint8_t* alpha, uint32_t stride);
    IDWriteTextFormat* FmtFor(uint16_t attrs) const;

    // D3D
    ComPtr<ID3D11Device>             dev_;
    ComPtr<ID3D11DeviceContext>      ctx_;
    ComPtr<ID3D11Texture2D>          tex_;
    ComPtr<ID3D11ShaderResourceView> srv_;

    // Non-owning pointers to the four text formats (TerminalView owns them).
    IDWriteTextFormat* fmts_[4]{};   // 0=reg 1=bold 2=italic 3=bold+italic

    float cellW_{1.0f};
    float cellH_{1.0f};
    float font_size_px_{13.0f};

    std::vector<Shelf> shelves_;
    std::unordered_map<GlyphKey, GlyphSlot, GlyphKeyHash> cache_;
};

}  // namespace vrtx::render
