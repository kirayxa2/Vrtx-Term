#include "ui/SettingsView.h"

namespace mactw::ui {

namespace {

float EaseOutCubic(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float Smoothstep(float lo, float hi, float x) {
    const float t = std::clamp((x - lo) / std::max(1e-6f, hi - lo),
                               0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void EnsureFormat(IDWriteFactory* dwrite,
                  ComPtr<IDWriteTextFormat>& fmt,
                  float& builtAt,
                  const wchar_t* family,
                  float wantSize,
                  DWRITE_FONT_WEIGHT weight,
                  DWRITE_TEXT_ALIGNMENT align,
                  DWRITE_PARAGRAPH_ALIGNMENT pAlign) {
    if (!dwrite) return;
    if (fmt && std::abs(builtAt - wantSize) <= 0.5f) return;

    fmt.Reset();
    dwrite->CreateTextFormat(
        family, nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        wantSize, L"en-us",
        fmt.GetAddressOf());
    if (fmt) {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        fmt->SetTextAlignment(align);
        fmt->SetParagraphAlignment(pAlign);
    }
    builtAt = wantSize;
}

}  // namespace

// ---------------------------------------------------------------------------

void SettingsView::UpdateLayout(D2D1_RECT_F squircleRect,
                                float captionHeightPx, UINT dpi) {
    using namespace theme;

    sidebar_radius_px_  = ToPx(kSettingsSidebarRadius,    dpi);
    row_h_px_           = ToPx(kSettingsRowHeight,        dpi);
    row_gap_px_         = ToPx(kSettingsRowGap,           dpi);
    row_pad_x_px_       = ToPx(kSettingsRowPaddingX,      dpi);
    row_radius_px_      = ToPx(kSettingsRowRadius,        dpi);
    row_side_pad_px_    = ToPx(kSettingsRowSidePadding,   dpi);
    tile_size_px_       = ToPx(kSettingsTileSize,         dpi);
    tile_radius_px_     = ToPx(kSettingsTileRadius,       dpi);
    tile_glyph_px_      = ToPx(kSettingsTileGlyphSize,    dpi);
    tile_text_gap_px_   = ToPx(kSettingsTileTextGap,      dpi);
    row_text_px_        = ToPx(kSettingsRowTextSize,      dpi);
    sidebar_top_gap_px_ = ToPx(kSettingsSidebarTopGap,    dpi);
    sidebar_bot_gap_px_ = ToPx(kSettingsSidebarBottomGap, dpi);
    content_pad_x_px_   = ToPx(kSettingsContentPaddingX,  dpi);
    content_pad_top_px_ = ToPx(kSettingsContentPaddingTop,dpi);
    title_text_px_      = ToPx(kSettingsTitleSize,        dpi);
    title_bot_gap_px_   = ToPx(kSettingsTitleBottomGap,   dpi);
    body_text_px_       = ToPx(kSettingsBodyTextSize,     dpi);
    done_text_px_       = ToPx(kSettingsDoneTextSize,     dpi);

    const float padX    = ToPx(kSettingsOuterPaddingX,    dpi);
    const float padTop  = ToPx(kSettingsOuterPaddingTop,  dpi);
    const float padBot  = ToPx(kSettingsOuterPaddingBot,  dpi);
    const float gap     = ToPx(kSettingsSidebarGap,       dpi);
    const float sbW     = ToPx(kSettingsSidebarWidth,     dpi);

    // Sidebar pill: floats inside the squircle below the caption strip.
    // Top edge is BELOW the caption strip + a small breathing gap so the
    // traffic-lights (which live in the strip) optically anchor onto the
    // pill's top edge - exactly like Tahoe System Settings.
    sidebar_.left   = squircleRect.left + padX;
    sidebar_.top    = squircleRect.top  + captionHeightPx + padTop;
    sidebar_.right  = sidebar_.left + sbW;
    sidebar_.bottom = squircleRect.bottom - padBot;

    // Content area: the rest of the squircle to the right, with a small
    // inset gutter. We intentionally do NOT fill it: settings controls
    // (and the title) sit directly on the window surface.
    content_.left   = sidebar_.right + gap;
    content_.top    = squircleRect.top + captionHeightPx;
    content_.right  = squircleRect.right - padX;
    content_.bottom = squircleRect.bottom - padBot;

    // Done pill: top-right of the squircle, replacing the chrome
    // chevron-button while the sheet is up. Its vertical centre lines
    // up with the caption strip's centre so the pill optically belongs
    // to the title bar rather than to the content.
    const float doneW    = ToPx(kSettingsDoneWidth,  dpi);
    const float doneH    = ToPx(kSettingsDoneHeight, dpi);
    const float doneInsetX = ToPx(kSettingsDoneInsetX, dpi);
    const float capCenterY = squircleRect.top + captionHeightPx * 0.5f
                           + ToPx(theme::kCaptionButtonOffsetY, dpi);
    done_btn_.right  = squircleRect.right - doneInsetX;
    done_btn_.left   = done_btn_.right - doneW;
    done_btn_.top    = capCenterY - doneH * 0.5f;
    done_btn_.bottom = capCenterY + doneH * 0.5f;
}

bool SettingsView::HitTestSidebar(int x, int y) const {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= sidebar_.left && fx < sidebar_.right
        && fy >= sidebar_.top  && fy < sidebar_.bottom;
}

bool SettingsView::DoneAt(int x, int y) const {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= done_btn_.left && fx < done_btn_.right &&
           fy >= done_btn_.top  && fy < done_btn_.bottom;
}

int SettingsView::RowAt(int x, int y) const {
    if (items_.empty()) return -1;
    if (!HitTestSidebar(x, y)) return -1;

    const float fy = static_cast<float>(y);
    const float rowsTop = sidebar_.top + sidebar_top_gap_px_;
    if (fy < rowsTop) return -1;

    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = rowsTop + static_cast<float>(i)
                          * (row_h_px_ + row_gap_px_);
        const float bot = top + row_h_px_;
        if (top >= sidebar_.bottom - sidebar_bot_gap_px_) break;
        if (fy >= top && fy < bot) return static_cast<int>(i);
    }
    return -1;
}

void SettingsView::OnMouseMove(int x, int y) {
    if (progress_ < 0.4f) {
        hover_index_ = -1;
        hover_done_  = false;
        return;
    }
    hover_done_  = DoneAt(x, y);
    hover_index_ = hover_done_ ? -1 : RowAt(x, y);
}

void SettingsView::OnMouseLeave() {
    hover_index_   = -1;
    pressed_index_ = -1;
    hover_done_    = false;
    pressed_done_  = false;
}

void SettingsView::OnLButtonDown(int x, int y) {
    pressed_done_  = DoneAt(x, y);
    pressed_index_ = pressed_done_ ? -1 : RowAt(x, y);
}

bool SettingsView::OnLButtonUp(int x, int y) {
    const bool fireDone = pressed_done_ && DoneAt(x, y);
    const int  upRow    = pressed_done_ ? -1 : RowAt(x, y);
    const int  fireRow  = (pressed_index_ >= 0 && pressed_index_ == upRow)
                            ? pressed_index_ : -1;
    pressed_done_  = false;
    pressed_index_ = -1;

    if (fireRow >= 0 && fireRow < static_cast<int>(items_.size())) {
        active_index_ = fireRow;
        if (items_[fireRow].on_pick) items_[fireRow].on_pick();
    }
    if (fireDone && on_done_) on_done_();
    return fireDone;
}

// ---------------------------------------------------------------------------

void SettingsView::Render(ID2D1DeviceContext* dc,
                          ID2D1SolidColorBrush* brush,
                          ID2D1Factory* /*factory*/,
                          IDWriteFactory* dwrite) const {
    if (!open_ && progress_ <= 0.0f) return;

    const auto& pal = theme::ActivePalette();
    const float ease = EaseOutCubic(progress_);

    // Subtle slide-from-bottom + fade. The whole sheet (sidebar +
    // content + Done) translates a few pt up as it lands.
    const float slidePx = (1.0f - ease) * 12.0f;

    D2D1_MATRIX_3X2_F prev;
    dc->GetTransform(&prev);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, slidePx) * prev);

    const float fade = ease;

    // ---- Sidebar pill --------------------------------------------------
    {
        const D2D1_ROUNDED_RECT panel{sidebar_,
                                      sidebar_radius_px_,
                                      sidebar_radius_px_};
        const auto fill = pal.settingsSidebarFill;
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillRoundedRectangle(panel, brush);
    }

    // ---- Done button (caption-strip pill) ------------------------------
    {
        // True pill: radius == half height.
        const float r = std::max(2.0f,
            (done_btn_.bottom - done_btn_.top) * 0.5f);
        const D2D1_ROUNDED_RECT btn{done_btn_, r, r};

        auto fill = pal.settingsDoneFill;
        if (pressed_done_) {
            fill.r = std::max(0.0f, fill.r * 0.92f);
            fill.g = std::max(0.0f, fill.g * 0.92f);
            fill.b = std::max(0.0f, fill.b * 0.92f);
        }
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillRoundedRectangle(btn, brush);

        if (hover_done_ && !pressed_done_) {
            const auto hv = pal.settingsDoneHover;
            brush->SetColor(D2D1::ColorF(hv.r, hv.g, hv.b, hv.a * fade));
            dc->FillRoundedRectangle(btn, brush);
        }
    }

    // Body content (rows / labels / title) settles in slightly behind
    // the pill so the sheet reads as "container first, controls after".
    const float contentFade = Smoothstep(0.3f, 1.0f, progress_);
    if (contentFade <= 0.0f) {
        dc->SetTransform(prev);
        return;
    }

    // Build text formats lazily.
    EnsureFormat(dwrite, row_fmt_,    built_at_row_,
                 L"Segoe UI", row_text_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (row_fmt_) row_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, tile_fmt_,   built_at_tile_,
                 L"Segoe UI Symbol", tile_glyph_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (tile_fmt_) tile_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, title_fmt_,  built_at_title_,
                 L"Segoe UI", title_text_px_,
                 DWRITE_FONT_WEIGHT_BOLD,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (title_fmt_) title_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, body_fmt_,   built_at_body_,
                 L"Segoe UI", body_text_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    EnsureFormat(dwrite, done_fmt_,   built_at_done_,
                 L"Segoe UI", done_text_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (done_fmt_) done_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    // ---- Done label ----------------------------------------------------
    if (done_fmt_) {
        const std::wstring& label = done_label_.empty()
                                        ? std::wstring(L"Done")
                                        : done_label_;
        const auto t = pal.settingsDoneText;
        brush->SetColor(D2D1::ColorF(t.r, t.g, t.b, t.a * contentFade));
        dc->DrawTextW(label.c_str(),
                      static_cast<UINT32>(label.size()),
                      done_fmt_.Get(), done_btn_, brush);
    }

    // ---- Sidebar rows --------------------------------------------------
    const float rowsTop = sidebar_.top + sidebar_top_gap_px_;
    const float pillL   = sidebar_.left  + row_side_pad_px_;
    const float pillR   = sidebar_.right - row_side_pad_px_;

    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = rowsTop
                        + static_cast<float>(i) * (row_h_px_ + row_gap_px_);
        const float bot = top + row_h_px_;
        if (top >= sidebar_.bottom - sidebar_bot_gap_px_) break;

        const auto& it = items_[i];
        const bool active  = (static_cast<int>(i) == active_index_);
        const bool hovered = (static_cast<int>(i) == hover_index_);

        // Row pill background.
        if (active || hovered) {
            const D2D1_ROUNDED_RECT pill{
                D2D1::RectF(pillL, top, pillR, bot),
                row_radius_px_, row_radius_px_,
            };
            const auto c = active ? pal.settingsRowActive
                                  : pal.settingsRowHover;
            brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * contentFade));
            dc->FillRoundedRectangle(pill, brush);
        }

        // Coloured tile + glyph.
        const float tileX = pillL + row_pad_x_px_;
        const float tileY = top + (row_h_px_ - tile_size_px_) * 0.5f;
        {
            const D2D1_ROUNDED_RECT tile{
                D2D1::RectF(tileX, tileY,
                            tileX + tile_size_px_,
                            tileY + tile_size_px_),
                tile_radius_px_, tile_radius_px_,
            };
            auto tc = it.tile_color;
            if (active) {
                // Tile sits on top of system blue when selected; bump
                // brightness a touch so the icon still pops.
                tc.r = std::min(1.0f, tc.r * 1.06f + 0.04f);
                tc.g = std::min(1.0f, tc.g * 1.06f + 0.04f);
                tc.b = std::min(1.0f, tc.b * 1.06f + 0.04f);
            }
            brush->SetColor(D2D1::ColorF(tc.r, tc.g, tc.b, tc.a * contentFade));
            dc->FillRoundedRectangle(tile, brush);

            if (tile_fmt_ && !it.glyph.empty()) {
                const auto g = pal.settingsTileGlyph;
                brush->SetColor(D2D1::ColorF(g.r, g.g, g.b, g.a * contentFade));
                const D2D1_RECT_F glyphRect{
                    tileX, tileY,
                    tileX + tile_size_px_, tileY + tile_size_px_,
                };
                dc->DrawTextW(it.glyph.c_str(),
                              static_cast<UINT32>(it.glyph.size()),
                              tile_fmt_.Get(), glyphRect, brush);
            }
        }

        // Row label.
        if (row_fmt_ && !it.label.empty()) {
            const auto txtCol = active ? pal.settingsRowTextActive
                                       : pal.settingsRowText;
            brush->SetColor(D2D1::ColorF(txtCol.r, txtCol.g, txtCol.b,
                                         txtCol.a * contentFade));
            const D2D1_RECT_F textRect{
                tileX + tile_size_px_ + tile_text_gap_px_,
                top, pillR - row_pad_x_px_, bot,
            };
            dc->DrawTextW(it.label.c_str(),
                          static_cast<UINT32>(it.label.size()),
                          row_fmt_.Get(), textRect, brush);
        }
    }

    // ---- Content pane: title + body for the active row -----------------
    //
    // The pane has no card / wrapper - text + future controls sit
    // directly on the squircle. There's only an inner gutter so they
    // don't huddle the right edge.
    if (active_index_ >= 0
        && active_index_ < static_cast<int>(items_.size())) {
        const auto& it = items_[active_index_];

        const float cx0 = content_.left  + content_pad_x_px_;
        const float cy0 = content_.top   + content_pad_top_px_;
        // Reserve a margin so the title can't slide under the Done pill.
        const float cx1 = std::min(content_.right - content_pad_x_px_,
                                   done_btn_.left - content_pad_x_px_ * 0.5f);

        if (title_fmt_ && !it.title.empty()) {
            const D2D1_RECT_F r{
                cx0, cy0, cx1, cy0 + title_text_px_ * 1.4f
            };
            const auto c = pal.settingsTitle;
            brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * contentFade));
            dc->DrawTextW(it.title.c_str(),
                          static_cast<UINT32>(it.title.size()),
                          title_fmt_.Get(), r, brush);
        }

        // Body paragraph - placeholder until the real controls land. No
        // card, no fill: it lives directly on the squircle, like the
        // section descriptions in macOS System Settings.
        if (body_fmt_ && !it.body.empty()) {
            const float bodyTop = cy0 + title_text_px_ * 1.4f
                                + title_bot_gap_px_;
            const float cardLeft   = content_.left  + content_pad_x_px_;
            const float cardRight  = content_.right - content_pad_x_px_;
            const float cardBottom = content_.bottom;
            if (bodyTop < cardBottom) {
                const D2D1_RECT_F textRect{
                    cardLeft, bodyTop,
                    cardRight, cardBottom,
                };
                const auto bc = pal.settingsBody;
                brush->SetColor(D2D1::ColorF(bc.r, bc.g, bc.b,
                                             bc.a * contentFade));
                dc->DrawTextW(it.body.c_str(),
                              static_cast<UINT32>(it.body.size()),
                              body_fmt_.Get(), textRect, brush);
            }
        }
    }

    dc->SetTransform(prev);
}

}  // namespace mactw::ui
