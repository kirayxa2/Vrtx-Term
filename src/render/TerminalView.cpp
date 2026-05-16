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

    struct Mapping {
        DWRITE_UNICODE_RANGE        range;
        std::vector<const wchar_t*> families;
    };

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
        L"Segoe UI Symbol",
        L"Segoe MDL2 Assets",
        L"Segoe Fluent Icons",
    };

    const Mapping mappings[] = {
        { {0xE000, 0xF8FF},   kNerdFonts },
        { {0xF0000, 0xFFFFD}, kNerdFonts },
        { {0x1F300, 0x1FAFF}, {L"Segoe UI Emoji", L"Segoe UI Symbol"} },
        { {0x2600,  0x27BF},  {L"Segoe UI Emoji", L"Segoe UI Symbol"} },
        { {0x2500,  0x259F},  {L"Cascadia Mono", L"Consolas",
                                L"DejaVu Sans Mono"} },
    };

    for (const auto& m : mappings) {
        std::vector<const wchar_t*> ptrs(m.families.begin(), m.families.end());
        builder->AddMapping(&m.range, 1,
                            ptrs.data(), static_cast<UINT32>(ptrs.size()),
                            /*fontCollection=*/nullptr,
                            /*localeName=*/nullptr,
                            /*baseFamily=*/nullptr,
                            /*scale=*/1.0f);
    }

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

    // Snapshot the visible viewport into a flat array of cells. This costs
    // one extra copy per frame but keeps the rest of the drawing code in
    // its existing flat-grid form, and the copy is microseconds for any
    // realistic grid size (200x60 = 12k cells, 16 bytes each = 192 KB).
    std::vector<terminal::Cell> cells(static_cast<size_t>(cols) * rows);
    for (int r = 0; r < rows; ++r) {
        buf.GetViewportRow(r, cells.data() + static_cast<size_t>(r) * cols);
    }

    // Selection highlight colour. Apple uses a translucent accent; we
    // pre-multiply because the swap chain is premul-alpha.
    const D2D1_COLOR_F selBg = D2D1::ColorF(0.27f, 0.42f, 0.83f, 0.85f);
    const D2D1_COLOR_F selFg = D2D1::ColorF(1.0f,  1.0f,  1.0f,  1.0f);

    // Whether the user is looking at the live bottom of the buffer. The
    // cursor only renders when this is true (we don't want it floating
    // over scrolled-back content).
    const bool atBottom = (buf.ViewportOffset() == 0);

    // ---- Pass 1: background runs --------------------------------------
    for (int r = 0; r < rows; ++r) {
        const float y    = originY + r * cell_h_px_;
        const int64_t ar = buf.ViewportRowToAbs(r);
        int c = 0;
        while (c < cols) {
            const auto& cell = cells[r * cols + c];
            const bool selected = buf.IsCellSelected(ar, c);
            const bool reverse  = (cell.attrs & terminal::attr::kReverse) != 0;

            D2D1_COLOR_F bg;
            bool drawBg;
            if (selected) {
                bg     = selBg;
                drawBg = true;
            } else {
                auto bgC = reverse ? cell.fg : cell.bg;
                bg = ResolveColor(bgC, pal, /*isBg=*/!reverse);
                drawBg = !((bgC.mode == terminal::CellColor::Mode::Default) &&
                           !reverse);
            }

            int runEnd = c + 1;
            while (runEnd < cols) {
                const auto& nx = cells[r * cols + runEnd];
                const bool nxSel = buf.IsCellSelected(ar, runEnd);
                const bool nxRev = (nx.attrs & terminal::attr::kReverse) != 0;

                D2D1_COLOR_F nxBg;
                bool nxDraw;
                if (nxSel) {
                    nxBg = selBg; nxDraw = true;
                } else {
                    auto nxBgC = nxRev ? nx.fg : nx.bg;
                    nxBg = ResolveColor(nxBgC, pal, /*isBg=*/!nxRev);
                    nxDraw = !((nxBgC.mode == terminal::CellColor::Mode::Default) &&
                               !nxRev);
                }

                if (nxDraw != drawBg)              break;
                if (drawBg && !ColorsEqual(nxBg, bg)) break;
                ++runEnd;
            }

            if (drawBg) {
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
    std::wstring runText;
    runText.reserve(static_cast<size_t>(cols) + 8);

    for (int r = 0; r < rows; ++r) {
        const float y    = originY + r * cell_h_px_;
        const int64_t ar = buf.ViewportRowToAbs(r);
        int c = 0;
        while (c < cols) {
            const auto& cell = cells[r * cols + c];
            const bool selected = buf.IsCellSelected(ar, c);
            const bool reverse  = (cell.attrs & terminal::attr::kReverse) != 0;

            D2D1_COLOR_F fg;
            if (selected) {
                fg = selFg;
            } else {
                const auto& fgSrc = reverse ? cell.bg : cell.fg;
                fg = ResolveColor(fgSrc, pal, /*isBg=*/reverse);
            }
            const uint16_t a = cell.attrs &
                               (terminal::attr::kBold | terminal::attr::kItalic);
            IDWriteTextFormat* fmt = FormatFor(a);

            int runEnd = c + 1;
            while (runEnd < cols) {
                const auto& nx = cells[r * cols + runEnd];
                const bool nxSel = buf.IsCellSelected(ar, runEnd);
                const bool nxRev = (nx.attrs & terminal::attr::kReverse) != 0;

                D2D1_COLOR_F nxFg;
                if (nxSel) {
                    nxFg = selFg;
                } else {
                    const auto& nxFgSrc = nxRev ? nx.bg : nx.fg;
                    nxFg = ResolveColor(nxFgSrc, pal, nxRev);
                }
                const uint16_t nxA = nx.attrs &
                                     (terminal::attr::kBold | terminal::attr::kItalic);
                if (!ColorsEqual(nxFg, fg) || nxA != a) break;
                ++runEnd;
            }

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
                dc->DrawTextW(runText.c_str(),
                              static_cast<UINT32>(runText.size()),
                              fmt, layoutRect, fillBrush.Get(),
                              D2D1_DRAW_TEXT_OPTIONS_CLIP |
                              D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }

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
    //
    // Only when the user is looking at the live bottom of the buffer.
    // Otherwise the cursor would float at a meaningless position over the
    // scrolled-back content.
    if (atBottom) {
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

            const auto& cellU = cells[static_cast<size_t>(cr) * cols + cc];
            char32_t cp = cellU.ch;
            if (cp != 0 && cp != U' ') {
                const auto& bgC = pal.terminalBg;
                fillBrush->SetColor(D2D1::ColorF(bgC.r, bgC.g, bgC.b, 1.0f));

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
