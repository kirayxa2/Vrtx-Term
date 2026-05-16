// Renders the terminal grid (cells + cursor) into a Direct2D context.
//
// MVP rendering strategy:
//
//   1. One IDWriteTextFormat per (weight x style) combination, all keyed
//      to the same monospace family. We measure cell metrics once at
//      Initialize() / OnDpiChanged() time using the natural advance of
//      "M" in the Regular format - good enough for a monospace face.
//
//   2. Per-frame, two passes per row:
//        a. Background pass: coalesce contiguous cells with the same bg
//           into a single FillRectangle.
//        b. Foreground pass: coalesce contiguous cells with the same
//           (fg, attrs) into a single DrawTextW call.
//      This is O(cells) work, no glyph-level allocations, and good for
//      80x24 ... 200x60 grids.
//
//   3. Cursor: a filled rectangle when focused, outline when not.
//
// Future optimisation (atlas + custom IDWriteTextRenderer) can drop in
// behind the same Draw() facade.

#pragma once

#include "pch.h"
#include "terminal/TerminalSession.h"

namespace vrtx::render {

class TerminalView {
public:
    void Initialize(IDWriteFactory* dwrite, UINT dpi);

    // Recreate the text formats for the new DPI. Cell metrics are
    // recomputed.
    void OnDpiChanged(UINT dpi);

    // Pixel size of one cell at the current DPI.
    float CellWidthPx()  const { return cell_w_px_; }
    float CellHeightPx() const { return cell_h_px_; }

    // Padding inside the content rect (already in physical px).
    float PaddingXPx() const { return pad_x_px_; }
    float PaddingYPx() const { return pad_y_px_; }

    // Compute the (cols, rows) that fit inside a content rectangle of the
    // given physical pixel size, after subtracting padding. Both clamped
    // to >= 1.
    void GridForContent(float contentWpx, float contentHpx,
                        int& cols, int& rows) const;

    // Draw the grid for `session` into the rect (squircle-local px,
    // already inside the squircle clip layer).
    void Draw(ID2D1DeviceContext* dc,
              terminal::TerminalSession& session,
              D2D1_RECT_F contentRectPx,
              bool focused);

private:
    void RebuildFormats();
    IDWriteTextFormat* FormatFor(uint16_t attrs) const;

    void BuildFontFallback();

    ComPtr<IDWriteFactory>    dwrite_;
    ComPtr<IDWriteFactory2>   dwrite2_;          // for IDWriteFontFallback
    ComPtr<IDWriteFontFallback> fallback_;        // glyph fallback to Nerd / emoji fonts
    ComPtr<IDWriteTextFormat> fmt_regular_;
    ComPtr<IDWriteTextFormat> fmt_bold_;
    ComPtr<IDWriteTextFormat> fmt_italic_;
    ComPtr<IDWriteTextFormat> fmt_bold_italic_;

    UINT  dpi_       {96};
    float font_size_px_{13.0f};
    float cell_w_px_   {8.0f};
    float cell_h_px_   {16.0f};
    float baseline_px_ {12.0f};
    float pad_x_px_    {12.0f};
    float pad_y_px_    {8.0f};
};

}  // namespace vrtx::render
