#include "render/TerminalView.h"

#include "render/BoxDrawing.h"
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
    // IDWriteFactory2 (Win 8.1+) is needed for IDWriteFontFallbackBuilder.
    // Every Win10 box has it, so this should never fail in practice.
    dwrite_.As(&dwrite2_);
    dpi_    = dpi;
    BuildFontFallback();
    RebuildFormats();
}

void TerminalView::OnDpiChanged(UINT dpi) {
    dpi_ = dpi;
    RebuildFormats();
}

// ---- Font fallback -------------------------------------------------------
//
// PowerShell prompts (oh-my-posh, Powerlevel10k, Starship) and `ls --icons`
// rely heavily on Nerd Font glyphs - icons that live in Unicode Private Use
// Areas (U+E000..U+F8FF and U+F0000..U+FFFFD). The default DirectWrite
// fallback chain is "system fonts only" which doesn't include any Nerd
// Font, so those codepoints render as the missing-glyph box.
//
// We override the fallback with a small ranked list:
//
//   1. The user's primary font (Cascadia Code) - already covers ASCII +
//      most BMP punctuation + some symbols.
//   2. "Cascadia Code NF" / "CaskaydiaCove Nerd Font" - if the user has
//      installed any Cascadia Nerd Font variant. Same metrics as the base
//      face, so the icons line up perfectly with the grid.
//   3. "Symbols Nerd Font Mono" / "JetBrainsMono NF" / "FiraCode NF" /
//      "Hack NF" - other common Nerd Fonts; we list the most popular ones.
//   4. "Segoe UI Emoji" - colour emoji (U+1F300..U+1FAFF, etc.).
//   5. "Segoe UI Symbol" / "Segoe MDL2 Assets" - Microsoft's own symbol
//      sets, partial coverage of various dingbats and box-drawing.
//   6. The default system fallback as the absolute last resort.
//
// If a font in the list is not installed, AddMappings silently maps the
// range to nothing for that font and DirectWrite moves on to the next
// candidate.

void TerminalView::BuildFontFallback() {
    fallback_.Reset();
    if (!dwrite2_) return;

    ComPtr<IDWriteFontFallbackBuilder> builder;
    if (FAILED(dwrite2_->CreateFontFallbackBuilder(builder.GetAddressOf()))) {
        return;
    }

    // (range, font-family-list) pairs. AddMappings checks each family in
    // the order given; the first one that exists on the system wins.
    struct Mapping {
        DWRITE_UNICODE_RANGE        range;
        std::vector<const wchar_t*> families;
    };

    // Nerd Font icon ranges. Sources: https://www.nerdfonts.com/cheat-sheet
    //
    //   E000..E0FF  Powerline / Powerline Extra
    //   E0A0..E0D7  Powerline glyphs proper
    //   E200..E2A9  Font Awesome Extension
    //   E300..E3D2  Weather Icons
    //   E5FA..E62F  Seti UI / custom file icons
    //   E700..E7C5  Devicons
    //   F000..F2E0  Font Awesome
    //   F300..F385  Font Logos
    //   F400..F532  Octicons
    //   F500..F8FF  Material Design Icons (subset)
    //
    // Easier: just cover the entire PUA in one mapping. Anything in there
    // that the Nerd Font *doesn't* cover, the chain falls through to
    // Segoe UI Symbol / system fallback.
    const std::vector<const wchar_t*> kNerdFonts = {
        L"CaskaydiaCove Nerd Font Mono",
        L"CaskaydiaCove Nerd Font",
        L"Cascadia Code NF",
        L"Cascadia Mono NF",
        L"JetBrainsMono Nerd Font Mono",
        L"JetBrainsMono Nerd Font",
        L"JetBrainsMono NF",
        L"FiraCode Nerd Font Mono",
        L"FiraCode NF",
        L"Hack Nerd Font Mono",
        L"Hack NF",
        L"Symbols Nerd Font Mono",
        L"Symbols Nerd Font",
        // Microsoft fallbacks (always installed on Win10+):
        L"Segoe UI Symbol",
        L"Segoe MDL2 Assets",
        L"Segoe Fluent Icons",
    };

    const Mapping mappings[] = {
        // Primary BMP PUA - where most Nerd Font icons live.
        { {0xE000, 0xF8FF}, kNerdFonts },
        // Supplementary PUA-A - some Material Design Icons live here.
        { {0xF0000, 0xFFFFD}, kNerdFonts },
        // Emoji blocks. Listed separately so colour-emoji fonts win.
        { {0x1F300, 0x1FAFF}, {L"Segoe UI Emoji", L"Segoe UI Symbol"} },
        { {0x2600,  0x27BF},  {L"Segoe UI Emoji", L"Segoe UI Symbol"} },
        // Box-drawing + block elements. Cascadia covers these but if the
        // user picks a font that doesn't, fall through to a known-good one.
        { {0x2500,  0x259F},  {L"Cascadia Mono", L"Consolas",
                                L"DejaVu Sans Mono"} },
    };

    for (const auto& m : mappings) {
        // Note: this is AddMapping (singular), not AddMappings. The
        // plural AddMappings in this interface takes a single
        // IDWriteFontFallback* and merges all its mappings. AddMapping
        // is the one that takes range + family list.
        std::vector<const wchar_t*> ptrs(m.families.begin(), m.families.end());
        builder->AddMapping(&m.range, 1,
                            ptrs.data(), static_cast<UINT32>(ptrs.size()),
                            /*fontCollection=*/nullptr,
                            /*localeName=*/nullptr,
                            /*baseFamily=*/nullptr,
                            /*scale=*/1.0f);
    }

    // Append the system default as the very last resort. Without this
    // step our custom fallback REPLACES the system one, and any glyph
    // outside the ranges above falls back to nothing.
    ComPtr<IDWriteFontFallback> systemFallback;
    if (SUCCEEDED(dwrite2_->GetSystemFontFallback(systemFallback.GetAddressOf()))) {
        builder->AddMappings(systemFallback.Get());
    }

    builder->CreateFontFallback(fallback_.GetAddressOf());
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

        // Apply our Nerd Font + emoji fallback chain. Requires
        // IDWriteTextFormat2 (Win 8.1+); QI silently no-ops on older
        // platforms, where users will still see boxes for icons.
        if (fallback_) {
            ComPtr<IDWriteTextFormat2> tf2;
            if (SUCCEEDED(dst.As(&tf2)) && tf2) {
                tf2->SetFontFallback(fallback_.Get());
            }
        }
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

            // Build the wide-string for the run. Box-drawing and block
            // glyphs are rendered ourselves via FillRectangle so they tile
            // seamlessly across cells regardless of line-height; we set
            // the codepoint to U+0020 in the run-text so DirectWrite skips
            // it, then call box::DrawGlyph for that cell directly.
            runText.clear();
            bool anyVisible = false;
            for (int k = c; k < runEnd; ++k) {
                char32_t cp = cells[r * cols + k].ch;
                const bool boxLike =
                    box::IsBoxDrawing(cp) || box::IsBlockElement(cp);
                if (!boxLike && cp != U' ' && cp != 0) anyVisible = true;

                const char32_t emit = boxLike ? U' ' : cp;
                if (emit <= 0xFFFF) {
                    runText.push_back(static_cast<wchar_t>(emit));
                } else {
                    char32_t e = emit - 0x10000;
                    runText.push_back(static_cast<wchar_t>(0xD800 + (e >> 10)));
                    runText.push_back(static_cast<wchar_t>(0xDC00 + (e & 0x3FF)));
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
                // CLIP keeps glyphs from spilling into neighbouring cells;
                // ENABLE_COLOR_FONT lets COLR/SVG fonts (Segoe UI Emoji,
                // and Nerd Fonts that ship a coloured Material Design set)
                // render in colour instead of being flattened to fg.
                dc->DrawTextW(runText.c_str(),
                              static_cast<UINT32>(runText.size()),
                              fmt, layoutRect, fillBrush.Get(),
                              D2D1_DRAW_TEXT_OPTIONS_CLIP |
                              D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }

            // Box-drawing pass for this run. Each glyph is independent
            // (no run-coalescing) since they're cheap rectangles. Stroke
            // thickness scales with font size: roughly 7% of cell height
            // for "light" and 18% for "heavy", clamped to >= 1px so the
            // rasteriser doesn't drop them at small sizes.
            const float lightPx = std::max(1.0f, std::round(cell_h_px_ * 0.07f));
            const float heavyPx = std::max(2.0f, std::round(cell_h_px_ * 0.18f));
            for (int k = c; k < runEnd; ++k) {
                char32_t cp = cells[r * cols + k].ch;
                if (!box::IsBoxDrawing(cp) && !box::IsBlockElement(cp)) continue;
                fillBrush->SetColor(fg);
                D2D1_RECT_F cellRect{
                    originX + k * cell_w_px_,
                    y,
                    originX + (k + 1) * cell_w_px_,
                    y + cell_h_px_,
                };
                box::DrawGlyph(dc, fillBrush.Get(), cellRect, cp, lightPx, heavyPx);
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
                const auto& bgC = pal.terminalBg;
                fillBrush->SetColor(D2D1::ColorF(bgC.r, bgC.g, bgC.b, 1.0f));

                // Box-drawing under the cursor: redraw via our own rect
                // primitives so it tiles seamlessly with the next row.
                if (box::IsBoxDrawing(cp) || box::IsBlockElement(cp)) {
                    const float lightPx = std::max(1.0f, std::round(cell_h_px_ * 0.07f));
                    const float heavyPx = std::max(2.0f, std::round(cell_h_px_ * 0.18f));
                    box::DrawGlyph(dc, fillBrush.Get(), r2, cp, lightPx, heavyPx);
                } else {
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
                    IDWriteTextFormat* fmt = FormatFor(
                        cellU.attrs &
                        (terminal::attr::kBold | terminal::attr::kItalic));
                    dc->DrawTextW(buf2, len, fmt, r2, fillBrush.Get(),
                                  D2D1_DRAW_TEXT_OPTIONS_CLIP |
                                  D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                }
            }
        } else {
            dc->DrawRectangle(r2, fillBrush.Get(), 1.0f);
        }
    }
}

}  // namespace mactw::render
