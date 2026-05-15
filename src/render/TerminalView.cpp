#include "render/TerminalView.h"

#include "theme/TahoeTheme.h"

namespace mactw::render {

namespace {

D2D1_COLOR_F ResolveColor(const terminal::CellColor& c,
                          const theme::Palette& pal,
                          bool isBg) {
    using terminal::CellColor;
    switch (c.mode) {
        case CellColor::Mode::Default: {
            const auto& dc = isBg ? pal.terminalBg : pal.terminalFg;
            return D2D1::ColorF(dc.r, dc.g, dc.b, dc.a);
        }
        case CellColor::Mode::Indexed: {
            const auto& ac = pal.ansi[c.index & 0x0F];
            return D2D1::ColorF(ac.r, ac.g, ac.b, ac.a);
        }
        case CellColor::Mode::Rgb:
            return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
    }
    return D2D1::ColorF(0, 0, 0, 1);
}

bool ColorsEqual(D2D1_COLOR_F a, D2D1_COLOR_F b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}  // namespace

// ---------------------------------------------------------------------------

void TerminalView::Initialize(IDWriteFactory* dwrite, UINT dpi) {
    dwrite_ = dwrite;
    dpi_    = dpi;
    RebuildFormats();
}

void TerminalView::OnDpiChanged(UINT dpi) {
    dpi_ = dpi;
    RebuildFormats();
}

// ---- Format / metrics -----------------------------------------------------

void TerminalView::RebuildFormats() {
    if (!dwrite_) return;

    font_size_px_ = theme::ToPx(theme::kTerminalFontSize, dpi_);
    pad_x_px_     = theme::ToPx(theme::kTerminalPaddingX, dpi_);
    pad_y_px_     = theme::ToPx(theme::kTerminalPaddingY, dpi_);

    auto make = [&](DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE s,
                    ComPtr<IDWriteTextFormat>& dst) {
        dst.Reset();
        ThrowIfFailed(
            dwrite_->CreateTextFormat(
                theme::kTerminalFontFamily, nullptr,
                w, s, DWRITE_FONT_STRETCH_NORMAL,
                font_size_px_, L"en-us",
                dst.GetAddressOf()),
            "CreateTextFormat");
        dst->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        dst->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        dst->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    };

    make(DWRITE_FONT_WEIGHT_NORMAL,  DWRITE_FONT_STYLE_NORMAL, fmt_regular_);
    make(DWRITE_FONT_WEIGHT_BOLD,    DWRITE_FONT_STYLE_NORMAL, fmt_bold_);
    make(DWRITE_FONT_WEIGHT_NORMAL,  DWRITE_FONT_STYLE_ITALIC, fmt_italic_);
    make(DWRITE_FONT_WEIGHT_BOLD,    DWRITE_FONT_STYLE_ITALIC, fmt_bold_italic_);

    // ---- Cell metrics ---------------------------------------------------
    //
    // For a monospace face the advance of any printable ASCII glyph equals
    // the cell width. We measure "M" in the Regular format. Height comes
    // from font metrics, scaled to design units, then multiplied by our
    // line-height factor.
    ComPtr<IDWriteTextLayout> probe;
    const wchar_t kSample[] = L"M";
    ThrowIfFailed(dwrite_->CreateTextLayout(
                      kSample, 1, fmt_regular_.Get(),
                      4096.0f, 4096.0f,
                      probe.GetAddressOf()),
                  "CreateTextLayout(probe)");

    DWRITE_TEXT_METRICS tm{};
    probe->GetMetrics(&tm);
    cell_w_px_ = std::max(1.0f, tm.widthIncludingTrailingWhitespace);

    // Accurate font metrics for the cell height + baseline.
    ComPtr<IDWriteFontCollection> fontColl;
    fmt_regular_->GetFontCollection(fontColl.GetAddressOf());

    wchar_t familyName[128] = {};
    fmt_regular_->GetFontFamilyName(familyName, 128);

    UINT32 idx = 0;
    BOOL   exists = FALSE;
    if (fontColl) {
        fontColl->FindFamilyName(familyName, &idx, &exists);
    }

    DWRITE_FONT_METRICS metrics{};
    if (exists) {
        ComPtr<IDWriteFontFamily> family;
        fontColl->GetFontFamily(idx, family.GetAddressOf());

        ComPtr<IDWriteFont> font;
        family->GetFirstMatchingFont(
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            font.GetAddressOf());
        if (font) font->GetMetrics(&metrics);
    }

    if (metrics.designUnitsPerEm > 0) {
        const float scale = font_size_px_ / metrics.designUnitsPerEm;
        const float ascent  = metrics.ascent  * scale;
        const float descent = metrics.descent * scale;
        const float gap     = metrics.lineGap * scale;
        cell_h_px_   = std::ceil((ascent + descent + gap) *
                                 theme::kTerminalLineHeight);
        baseline_px_ = ascent;
    } else {
        // Fallback if the font lookup failed for any reason.
        cell_h_px_   = std::ceil(font_size_px_ * 1.6f);
        baseline_px_ = font_size_px_ * 0.8f;
    }
}

IDWriteTextFormat* TerminalView::FormatFor(uint16_t attrs) const {
    const bool bold   = (attrs & terminal::attr::kBold)   != 0;
    const bool italic = (attrs & terminal::attr::kItalic) != 0;
    if (bold && italic) return fmt_bold_italic_.Get();
    if (bold)           return fmt_bold_.Get();
    if (italic)         return fmt_italic_.Get();
    return fmt_regular_.Get();
}

// ---------------------------------------------------------------------------

void TerminalView::GridForContent(float w, float h, int& cols, int& rows) const {
    const float innerW = std::max(1.0f, w - 2.0f * pad_x_px_);
    const float innerH = std::max(1.0f, h - 2.0f * pad_y_px_);
    cols = std::max(1, static_cast<int>(innerW / cell_w_px_));
    rows = std::max(1, static_cast<int>(innerH / cell_h_px_));
}

// ---------------------------------------------------------------------------

void TerminalView::Draw(ID2D1DeviceContext* dc,
                        terminal::TerminalSession& session,
                        D2D1_RECT_F rect,
                        bool focused) {
    if (!fmt_regular_) return;

    auto& buf = session.Buffer();
    std::lock_guard<std::mutex> lk(buf.Lock());

    const int cols = buf.Cols();
    const int rows = buf.Rows();
    if (cols <= 0 || rows <= 0) return;

    const auto& pal = theme::ActivePalette();

    const float originX = rect.left + pad_x_px_;
    const float originY = rect.top  + pad_y_px_;

    ComPtr<ID2D1SolidColorBrush> fillBrush;
    dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), fillBrush.GetAddressOf());

    const terminal::Cell* cells = buf.Cells();

    // ---- Pass 1: background runs --------------------------------------
    //
    // Coalesce contiguous cells that share the same effective bg into one
    // FillRectangle. The "default" bg matches the squircle fill that's
    // already on the canvas, so we *skip* drawing those runs entirely -
    // saves ~80% of fills for typical shell output.
    for (int r = 0; r < rows; ++r) {
        const float y = originY + r * cell_h_px_;
        int c = 0;
        while (c < cols) {
            const auto& cell = cells[r * cols + c];
            const bool reverse = (cell.attrs & terminal::attr::kReverse) != 0;
            auto bgC = reverse ? cell.fg : cell.bg;
            const D2D1_COLOR_F bg = ResolveColor(bgC, pal, /*isBg=*/!reverse);

            // Skip default bg (== window tint), no need to fill.
            const bool isDefaultBg = (bgC.mode == terminal::CellColor::Mode::Default) &&
                                     !reverse;

            int runEnd = c + 1;
            while (runEnd < cols) {
                const auto& nx = cells[r * cols + runEnd];
                const bool nxRev = (nx.attrs & terminal::attr::kReverse) != 0;
                auto nxBgC = nxRev ? nx.fg : nx.bg;
                const D2D1_COLOR_F nxBg = ResolveColor(nxBgC, pal, /*isBg=*/!nxRev);
                if (!ColorsEqual(nxBg, bg)) break;
                ++runEnd;
            }

            if (!isDefaultBg) {
                fillBrush->SetColor(bg);
                D2D1_RECT_F r2{
                    originX + c        * cell_w_px_,
                    y,
                    originX + runEnd   * cell_w_px_,
                    y + cell_h_px_,
                };
                dc->FillRectangle(r2, fillBrush.Get());
            }
            c = runEnd;
        }
    }

    // ---- Pass 2: glyph runs --------------------------------------------
    //
    // For each row, group runs of cells with identical (fg, bold, italic).
    // Skip purely-blank runs (no need to render space glyphs over an
    // already-painted background). Build a UTF-16 string and DrawTextW it
    // at the run's origin. DirectWrite handles all kerning/anti-aliasing.
    std::wstring runText;
    runText.reserve(static_cast<size_t>(cols) + 8);

    for (int r = 0; r < rows; ++r) {
        const float y = originY + r * cell_h_px_;
        int c = 0;
        while (c < cols) {
            const auto& cell = cells[r * cols + c];
            const bool reverse = (cell.attrs & terminal::attr::kReverse) != 0;
            const auto& fgSrc  = reverse ? cell.bg : cell.fg;
            const D2D1_COLOR_F fg = ResolveColor(fgSrc, pal, /*isBg=*/reverse);
            const uint16_t a = cell.attrs &
                               (terminal::attr::kBold | terminal::attr::kItalic);
            IDWriteTextFormat* fmt = FormatFor(a);

            int runEnd = c + 1;
            while (runEnd < cols) {
                const auto& nx = cells[r * cols + runEnd];
                const bool nxRev = (nx.attrs & terminal::attr::kReverse) != 0;
                const auto& nxFg = nxRev ? nx.bg : nx.fg;
                const D2D1_COLOR_F nxFgC = ResolveColor(nxFg, pal, nxRev);
                const uint16_t nxA = nx.attrs &
                                     (terminal::attr::kBold | terminal::attr::kItalic);
                if (!ColorsEqual(nxFgC, fg) || nxA != a) break;
                ++runEnd;
            }

            // Build the wide-string for the run.
            runText.clear();
            bool anyVisible = false;
            for (int k = c; k < runEnd; ++k) {
                char32_t cp = cells[r * cols + k].ch;
                if (cp != U' ' && cp != 0) anyVisible = true;
                if (cp <= 0xFFFF) {
                    runText.push_back(static_cast<wchar_t>(cp));
                } else {
                    cp -= 0x10000;
                    runText.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
                    runText.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
                }
            }

            if (anyVisible) {
                fillBrush->SetColor(fg);
                D2D1_RECT_F layoutRect{
                    originX + c * cell_w_px_,
                    y,
                    originX + runEnd * cell_w_px_,
                    y + cell_h_px_,
                };
                dc->DrawTextW(runText.c_str(),
                              static_cast<UINT32>(runText.size()),
                              fmt, layoutRect, fillBrush.Get(),
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }

            c = runEnd;
        }
    }

    // ---- Pass 3: cursor ------------------------------------------------
    {
        const int cr = std::clamp(buf.CursorRow(), 0, rows - 1);
        const int cc = std::clamp(buf.CursorCol(), 0, cols - 1);
        const D2D1_RECT_F r2{
            originX + cc * cell_w_px_,
            originY + cr * cell_h_px_,
            originX + (cc + 1) * cell_w_px_,
            originY + (cr + 1) * cell_h_px_,
        };
        const auto& cur = pal.cursor;
        fillBrush->SetColor(D2D1::ColorF(cur.r, cur.g, cur.b, cur.a));
        if (focused) {
            dc->FillRectangle(r2, fillBrush.Get());

            // Re-draw the glyph under the cursor in the bg colour, so it
            // remains legible inside the inverted block.
            const auto& cellU = cells[cr * cols + cc];
            char32_t cp = cellU.ch;
            if (cp != 0 && cp != U' ') {
                wchar_t buf2[2];
                int len = 0;
                if (cp <= 0xFFFF) {
                    buf2[0] = static_cast<wchar_t>(cp);
                    len = 1;
                } else {
                    cp -= 0x10000;
                    buf2[0] = static_cast<wchar_t>(0xD800 + (cp >> 10));
                    buf2[1] = static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
                    len = 2;
                }
                const auto& bgC = pal.terminalBg;
                fillBrush->SetColor(D2D1::ColorF(bgC.r, bgC.g, bgC.b, 1.0f));
                IDWriteTextFormat* fmt = FormatFor(
                    cellU.attrs &
                    (terminal::attr::kBold | terminal::attr::kItalic));
                dc->DrawTextW(buf2, len, fmt, r2, fillBrush.Get(),
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
        } else {
            dc->DrawRectangle(r2, fillBrush.Get(), 1.0f);
        }
    }
}

}  // namespace mactw::render
