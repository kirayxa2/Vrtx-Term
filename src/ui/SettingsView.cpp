#include "ui/SettingsView.h"

#include "window/SquircleGeometry.h"

namespace vrtx::ui {

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
    sidebar_border_px_  = ToPx(kSettingsSidebarBorderWidth,dpi);
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
    sidebar_top_gap_px_ = captionHeightPx + ToPx(kSettingsRowsTopGap, dpi);
    sidebar_bot_gap_px_ = ToPx(kSettingsSidebarBottomGap, dpi);
    content_pad_x_px_   = ToPx(kSettingsContentPaddingX,  dpi);
    content_pad_top_px_ = ToPx(kSettingsContentPaddingTop,dpi);
    title_text_px_      = ToPx(kSettingsTitleSize,        dpi);
    title_bot_gap_px_   = ToPx(kSettingsTitleBottomGap,   dpi);
    done_text_px_       = ToPx(kSettingsDoneTextSize,     dpi);

    card_radius_px_         = ToPx(kSettingsCardRadius,         dpi);
    card_spacing_px_        = ToPx(kSettingsCardSpacing,        dpi);
    card_pad_x_px_          = ToPx(kSettingsCardPaddingX,       dpi);
    card_row_h_px_          = ToPx(kSettingsCardRowHeight,      dpi);
    card_sep_inset_px_      = ToPx(kSettingsCardSeparatorInset, dpi);
    card_sep_w_px_          = ToPx(kSettingsCardSeparatorWidth, dpi);
    section_header_px_      = ToPx(kSettingsSectionHeaderSize,  dpi);
    section_header_gap_px_  = ToPx(kSettingsSectionHeaderGap,   dpi);
    row_label_px_           = ToPx(kSettingsRowLabelSize,       dpi);
    row_value_px_           = ToPx(kSettingsRowValueSize,       dpi);
    row_chev_px_            = ToPx(kSettingsRowChevronSize,     dpi);
    row_chev_gap_px_        = ToPx(kSettingsRowChevronGap,      dpi);
    row_end_pad_px_         = ToPx(kSettingsRowEndPadding,      dpi);
    toggle_w_px_            = ToPx(kSettingsToggleWidth,        dpi);
    toggle_h_px_            = ToPx(kSettingsToggleHeight,       dpi);
    toggle_knob_px_         = ToPx(kSettingsToggleKnob,         dpi);
    toggle_knob_inset_px_   = ToPx(kSettingsToggleKnobInset,    dpi);
    footer_text_px_         = ToPx(kSettingsFooterTextSize,     dpi);
    footer_top_gap_px_      = ToPx(kSettingsFooterTopGap,       dpi);
    footer_bot_gap_px_      = ToPx(kSettingsFooterBottomGap,    dpi);

    const float padL    = ToPx(kSettingsOuterPaddingLeft,  dpi);
    const float padR    = ToPx(kSettingsOuterPaddingRight, dpi);
    const float padTop  = ToPx(kSettingsOuterPaddingTop,   dpi);
    const float padBot  = ToPx(kSettingsOuterPaddingBot,   dpi);
    const float gap     = ToPx(kSettingsSidebarGap,        dpi);
    const float sbW     = ToPx(kSettingsSidebarWidth,      dpi);

    // Sidebar pill: top edge sits just below the squircle top (the
    // traffic-lights live inside the caption strip and get absorbed
    // into the pill's chrome - exactly like Vrtx Term System
    // Settings).
    sidebar_.left   = squircleRect.left  + padL;
    sidebar_.top    = squircleRect.top   + padTop;
    sidebar_.right  = sidebar_.left + sbW;
    sidebar_.bottom = squircleRect.bottom - padBot;

    // Content area: the rest of the squircle. Bare - no card / wrapper.
    content_.left   = sidebar_.right + gap;
    content_.top    = squircleRect.top + captionHeightPx;
    content_.right  = squircleRect.right - padR;
    content_.bottom = squircleRect.bottom - padBot;

    // Done pill: top-right of caption strip, replacing the chrome
    // chevron-button while the sheet is up.
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

int SettingsView::SidebarRowAt(int x, int y) const {
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

bool SettingsView::ContentRowRect(int section, int row,
                                  D2D1_RECT_F& out) const {
    if (active_index_ < 0
        || active_index_ >= static_cast<int>(items_.size())) return false;
    const auto& it = items_[active_index_];
    if (section < 0 || section >= static_cast<int>(it.sections.size()))
        return false;
    const auto& sec = it.sections[section];
    if (row < 0 || row >= static_cast<int>(sec.rows.size())) return false;

    const float left  = content_.left  + content_pad_x_px_;
    const float right = std::min(content_.right - content_pad_x_px_,
                                 done_btn_.left - content_pad_x_px_ * 0.5f);

    // Walk all sections above to find this card's vertical origin.
    float y = content_.top + content_pad_top_px_
            + title_text_px_ * 1.4f + title_bot_gap_px_;

    for (int s = 0; s < static_cast<int>(it.sections.size()); ++s) {
        const auto& cs = it.sections[s];
        if (!cs.header.empty()) {
            y += section_header_px_ * 1.2f + section_header_gap_px_;
        }
        const float cardTop = y;
        const float cardH   = card_row_h_px_
                            * static_cast<float>(cs.rows.size());
        if (s == section) {
            const float top = cardTop
                            + static_cast<float>(row) * card_row_h_px_;
            out = D2D1::RectF(left, top, right, top + card_row_h_px_);
            return true;
        }
        y += cardH;
        if (!cs.footer.empty()) {
            y += footer_top_gap_px_ + footer_text_px_ * 1.4f
               + footer_bot_gap_px_;
        }
        y += card_spacing_px_;
    }
    return false;
}

SettingsView::RowPath SettingsView::ContentRowAt(int x, int y) const {
    RowPath p;
    if (active_index_ < 0
        || active_index_ >= static_cast<int>(items_.size())) return p;
    const auto& it = items_[active_index_];
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);

    const float left  = content_.left  + content_pad_x_px_;
    const float right = std::min(content_.right - content_pad_x_px_,
                                 done_btn_.left - content_pad_x_px_ * 0.5f);
    if (fx < left || fx >= right) return p;

    float yc = content_.top + content_pad_top_px_
             + title_text_px_ * 1.4f + title_bot_gap_px_;
    for (int s = 0; s < static_cast<int>(it.sections.size()); ++s) {
        const auto& cs = it.sections[s];
        if (!cs.header.empty()) {
            yc += section_header_px_ * 1.2f + section_header_gap_px_;
        }
        const float cardTop = yc;
        const float cardH   = card_row_h_px_
                            * static_cast<float>(cs.rows.size());
        if (fy >= cardTop && fy < cardTop + cardH) {
            const int rowIdx = static_cast<int>(
                std::floor((fy - cardTop) / card_row_h_px_));
            if (rowIdx >= 0
                && rowIdx < static_cast<int>(cs.rows.size())) {
                p = {active_index_, s, rowIdx};
                return p;
            }
        }
        yc += cardH;
        if (!cs.footer.empty()) {
            yc += footer_top_gap_px_ + footer_text_px_ * 1.4f
                + footer_bot_gap_px_;
        }
        yc += card_spacing_px_;
    }
    return p;
}

void SettingsView::OnMouseMove(int x, int y) {
    if (progress_ < 0.4f) {
        hover_index_ = -1;
        hover_done_  = false;
        hover_row_path_ = {-1,-1,-1};
        return;
    }
    hover_done_  = DoneAt(x, y);
    hover_index_ = hover_done_ ? -1 : SidebarRowAt(x, y);
    hover_row_path_ = (hover_done_ || hover_index_ >= 0)
                        ? RowPath{-1,-1,-1}
                        : ContentRowAt(x, y);
}

void SettingsView::OnMouseLeave() {
    hover_index_   = -1;
    pressed_index_ = -1;
    hover_done_    = false;
    pressed_done_  = false;
    hover_row_path_   = {-1,-1,-1};
    pressed_row_path_ = {-1,-1,-1};
}

void SettingsView::OnLButtonDown(int x, int y) {
    pressed_done_  = DoneAt(x, y);
    if (pressed_done_) {
        pressed_index_     = -1;
        pressed_row_path_  = {-1,-1,-1};
        return;
    }
    pressed_index_ = SidebarRowAt(x, y);
    if (pressed_index_ >= 0) {
        pressed_row_path_ = {-1,-1,-1};
        return;
    }
    pressed_row_path_ = ContentRowAt(x, y);
}

bool SettingsView::OnLButtonUp(int x, int y) {
    const bool fireDone = pressed_done_ && DoneAt(x, y);
    const int  upRow    = pressed_done_ ? -1 : SidebarRowAt(x, y);
    const int  fireRow  = (pressed_index_ >= 0 && pressed_index_ == upRow)
                            ? pressed_index_ : -1;

    RowPath upContent{-1,-1,-1};
    if (!pressed_done_ && pressed_index_ < 0) upContent = ContentRowAt(x, y);
    const bool sameContent = pressed_row_path_.section >= 0
                          && pressed_row_path_.section == upContent.section
                          && pressed_row_path_.row     == upContent.row
                          && pressed_row_path_.item    == upContent.item;

    pressed_done_     = false;
    pressed_index_    = -1;
    const RowPath fireContent = sameContent ? pressed_row_path_
                                            : RowPath{-1,-1,-1};
    pressed_row_path_ = {-1,-1,-1};

    if (fireRow >= 0 && fireRow < static_cast<int>(items_.size())) {
        active_index_ = fireRow;
        if (items_[fireRow].on_pick) items_[fireRow].on_pick();
    }
    if (fireContent.item >= 0 && fireContent.item == active_index_) {
        Row* row = RowAt(fireContent.item,
                         fireContent.section, fireContent.row);
        if (row && row->enabled) {
            switch (row->kind) {
                case RowKind::Toggle:
                    row->toggle_state = !row->toggle_state;
                    if (row->on_toggle) row->on_toggle(row->toggle_state);
                    break;
                case RowKind::Plain:
                case RowKind::Value:
                    if (row->on_pick) row->on_pick();
                    break;
            }
        }
    }
    if (fireDone && on_done_) on_done_();
    return fireDone;
}

// ---------------------------------------------------------------------------

void SettingsView::Render(ID2D1DeviceContext* dc,
                          ID2D1SolidColorBrush* brush,
                          ID2D1Factory* factory,
                          IDWriteFactory* dwrite) const {
    if (!open_ && progress_ <= 0.0f) return;

    const auto& pal = theme::ActivePalette();
    const float ease = EaseOutCubic(progress_);

    // Subtle slide-from-bottom + fade.
    const float slidePx = (1.0f - ease) * 12.0f;

    D2D1_MATRIX_3X2_F prev;
    dc->GetTransform(&prev);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, slidePx) * prev);

    const float fade = ease;

    // ---- Sidebar pill (squircle fill + 1pt hairline outline) -----------
    //
    // We use the same continuous-corner geometry as the window itself,
    // so the pill reads as carved from the same chrome family rather
    // than as a foreign rounded rectangle floating inside it.
    if (factory) {
        auto pillGeom = window::BuildSquirclePathInRect(
            factory, sidebar_, sidebar_radius_px_,
            theme::kSquircleSmoothing);

        const auto fill = pal.settingsSidebarFill;
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillGeometry(pillGeom.Get(), brush);

        // Hairline matching the window border. Inset by half the stroke
        // width so D2D's centred stroke ends up fully inside the pill -
        // same trick we use for the squircle window outline.
        const float strokeW = std::max(1.0f, sidebar_border_px_);
        const float inset   = strokeW * 0.5f;
        D2D1_RECT_F outerR = sidebar_;
        outerR.left   += inset;
        outerR.top    += inset;
        outerR.right  -= inset;
        outerR.bottom -= inset;
        auto outlineGeom = window::BuildSquirclePathInRect(
            factory, outerR,
            std::max(0.0f, sidebar_radius_px_ - inset),
            theme::kSquircleSmoothing);

        const auto bc = pal.settingsSidebarBorder;
        brush->SetColor(D2D1::ColorF(bc.r, bc.g, bc.b, bc.a * fade));
        dc->DrawGeometry(outlineGeom.Get(), brush, strokeW);
    }

    // ---- Done button (caption-strip pill) ------------------------------
    {
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

    // Body content ramps in slightly later than the chrome.
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

    EnsureFormat(dwrite, done_fmt_,   built_at_done_,
                 L"Segoe UI", done_text_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (done_fmt_) done_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, header_fmt_, built_at_header_,
                 L"Segoe UI", section_header_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (header_fmt_) header_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, label_fmt_,  built_at_label_,
                 L"Segoe UI", row_label_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (label_fmt_) label_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, value_fmt_,  built_at_value_,
                 L"Segoe UI", row_value_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_TRAILING,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (value_fmt_) value_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, chev_fmt_,   built_at_chev_,
                 L"Segoe UI Symbol", row_chev_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (chev_fmt_) chev_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, footer_fmt_, built_at_footer_,
                 L"Segoe UI", footer_text_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

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

    // ---- Content pane: title + sections (cards) ------------------------
    if (active_index_ >= 0
        && active_index_ < static_cast<int>(items_.size())) {
        const auto& it = items_[active_index_];

        const float cx0 = content_.left  + content_pad_x_px_;
        const float cy0 = content_.top   + content_pad_top_px_;
        const float cx1 = std::min(content_.right - content_pad_x_px_,
                                   done_btn_.left - content_pad_x_px_ * 0.5f);

        // Title.
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

        // Sections (cards).
        float yc = cy0 + title_text_px_ * 1.4f + title_bot_gap_px_;
        for (int s = 0; s < static_cast<int>(it.sections.size()); ++s) {
            const auto& sec = it.sections[s];

            // Section header (small, all-caps style, secondary text).
            if (header_fmt_ && !sec.header.empty()) {
                const D2D1_RECT_F r{
                    cx0, yc, cx1, yc + section_header_px_ * 1.2f,
                };
                const auto c = pal.settingsSectionHeader;
                brush->SetColor(D2D1::ColorF(c.r, c.g, c.b,
                                             c.a * contentFade));
                dc->DrawTextW(sec.header.c_str(),
                              static_cast<UINT32>(sec.header.size()),
                              header_fmt_.Get(), r, brush);
                yc += section_header_px_ * 1.2f + section_header_gap_px_;
            }

            // Card geometry. Cards use the same continuous-corner
            // (squircle) shape as the window and the sidebar pill so the
            // entire chrome family reads as one consistent surface.
            const float cardTop = yc;
            const float cardH   = card_row_h_px_
                                * static_cast<float>(sec.rows.size());
            const float cardBot = cardTop + cardH;
            if (cardH > 0.0f) {
                const D2D1_RECT_F cardRect{cx0, cardTop, cx1, cardBot};
                const auto fc = pal.settingsCardFill;
                brush->SetColor(D2D1::ColorF(fc.r, fc.g, fc.b,
                                             fc.a * contentFade));
                if (factory) {
                    auto cardGeom = window::BuildSquirclePathInRect(
                        factory, cardRect, card_radius_px_,
                        theme::kSquircleSmoothing);
                    dc->FillGeometry(cardGeom.Get(), brush);

                    // 1pt hairline border, inset by half stroke width.
                    const float strokeW = std::max(1.0f, card_sep_w_px_);
                    const float inset   = strokeW * 0.5f;
                    D2D1_RECT_F br = cardRect;
                    br.left   += inset;
                    br.top    += inset;
                    br.right  -= inset;
                    br.bottom -= inset;
                    auto borderGeom = window::BuildSquirclePathInRect(
                        factory, br,
                        std::max(0.0f, card_radius_px_ - inset),
                        theme::kSquircleSmoothing);
                    const auto bc = pal.settingsCardBorder;
                    brush->SetColor(D2D1::ColorF(bc.r, bc.g, bc.b,
                                                 bc.a * contentFade));
                    dc->DrawGeometry(borderGeom.Get(), brush, strokeW);
                } else {
                    // Fallback: plain rounded rect (only hit if a caller
                    // did not pass a factory, which the renderer always
                    // does).
                    const D2D1_ROUNDED_RECT card{cardRect,
                                                 card_radius_px_,
                                                 card_radius_px_};
                    dc->FillRoundedRectangle(card, brush);
                    const float strokeW = std::max(1.0f, card_sep_w_px_);
                    const float inset   = strokeW * 0.5f;
                    D2D1_RECT_F br = cardRect;
                    br.left   += inset;
                    br.top    += inset;
                    br.right  -= inset;
                    br.bottom -= inset;
                    const float brr = std::max(0.0f,
                                               card_radius_px_ - inset);
                    const D2D1_ROUNDED_RECT bord{br, brr, brr};
                    const auto bc = pal.settingsCardBorder;
                    brush->SetColor(D2D1::ColorF(bc.r, bc.g, bc.b,
                                                 bc.a * contentFade));
                    dc->DrawRoundedRectangle(bord, brush, strokeW);
                }

                // Rows.
                for (int r = 0; r < static_cast<int>(sec.rows.size()); ++r) {
                    const auto& row = sec.rows[r];
                    const float rowTop = cardTop
                                       + static_cast<float>(r) * card_row_h_px_;
                    const float rowBot = rowTop + card_row_h_px_;

                    // Hover highlight (We use a subtle row highlight
                    // on tappable rows). Only show on rows that do
                    // something on click.
                    const bool tappable =
                        row.kind == RowKind::Toggle
                        || row.show_chevron
                        || static_cast<bool>(row.on_pick);
                    const bool hovered =
                        hover_row_path_.item == active_index_
                        && hover_row_path_.section == s
                        && hover_row_path_.row == r;
                    if (hovered && tappable) {
                        // Clip the highlight to the card's rounded
                        // rectangle so the top/bottom rows blend into
                        // the corner.
                        D2D1_RECT_F clipRect = cardRect;
                        dc->PushAxisAlignedClip(
                            clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        const auto hc = pal.settingsRowHover;
                        brush->SetColor(D2D1::ColorF(hc.r, hc.g, hc.b,
                                                     hc.a * contentFade));
                        dc->FillRectangle(
                            D2D1::RectF(cardRect.left, rowTop,
                                        cardRect.right, rowBot),
                            brush);
                        dc->PopAxisAlignedClip();
                    }

                    // Hairline separator below the row (not for the last
                    // one, and inset on the left like a grouped table).
                    if (r + 1 < static_cast<int>(sec.rows.size())) {
                        const auto sc = pal.settingsCardSeparator;
                        brush->SetColor(D2D1::ColorF(sc.r, sc.g, sc.b,
                                                     sc.a * contentFade));
                        const float sepY = rowBot - card_sep_w_px_ * 0.5f;
                        dc->FillRectangle(
                            D2D1::RectF(cardRect.left + card_sep_inset_px_,
                                        sepY,
                                        cardRect.right,
                                        sepY + card_sep_w_px_),
                            brush);
                    }

                    const float padL = cardRect.left + card_pad_x_px_;
                    const float padR = cardRect.right - card_pad_x_px_;

                    // Label.
                    if (label_fmt_ && !row.label.empty()) {
                        const auto lc = row.enabled
                            ? pal.settingsRowLabel
                            : pal.settingsRowValue;
                        brush->SetColor(D2D1::ColorF(lc.r, lc.g, lc.b,
                                                     lc.a * contentFade));
                        const D2D1_RECT_F lr{
                            padL, rowTop, padR, rowBot,
                        };
                        dc->DrawTextW(row.label.c_str(),
                                      static_cast<UINT32>(row.label.size()),
                                      label_fmt_.Get(), lr, brush);
                    }

                    // Trailing affordance: toggle / value / chevron.
                    float trailRight = padR;

                    if (row.kind == RowKind::Toggle) {
                        const float pillX = padR - toggle_w_px_;
                        const float pillY = rowTop
                                          + (card_row_h_px_ - toggle_h_px_) * 0.5f;
                        const D2D1_RECT_F pill{
                            pillX, pillY,
                            pillX + toggle_w_px_,
                            pillY + toggle_h_px_,
                        };
                        const float pr = toggle_h_px_ * 0.5f;
                        const D2D1_ROUNDED_RECT prr{pill, pr, pr};

                        const auto tc = row.toggle_state
                                            ? pal.settingsToggleOn
                                            : pal.settingsToggleOff;
                        brush->SetColor(D2D1::ColorF(tc.r, tc.g, tc.b,
                                                     tc.a * contentFade));
                        dc->FillRoundedRectangle(prr, brush);

                        // Knob: floats inset from the active edge.
                        const float kSize = toggle_knob_px_;
                        const float kInset = toggle_knob_inset_px_;
                        const float kX = row.toggle_state
                            ? pill.right  - kInset - kSize
                            : pill.left   + kInset;
                        const float kY = pillY
                                       + (toggle_h_px_ - kSize) * 0.5f;
                        const D2D1_ROUNDED_RECT knob{
                            D2D1::RectF(kX, kY, kX + kSize, kY + kSize),
                            kSize * 0.5f, kSize * 0.5f,
                        };
                        const auto kc = pal.settingsToggleKnob;
                        brush->SetColor(D2D1::ColorF(kc.r, kc.g, kc.b,
                                                     kc.a * contentFade));
                        dc->FillRoundedRectangle(knob, brush);

                        trailRight = pill.left - row_chev_gap_px_;
                    } else {
                        // Chevron (optional, drawn first because it
                        // anchors to the right edge).
                        if (row.show_chevron && chev_fmt_) {
                            const auto cc = pal.settingsRowChevron;
                            brush->SetColor(D2D1::ColorF(cc.r, cc.g, cc.b,
                                                         cc.a * contentFade));
                            const float chevW = row_chev_px_ * 0.9f;
                            const D2D1_RECT_F cr{
                                padR - chevW, rowTop,
                                padR, rowBot,
                            };
                            // U+203A SINGLE RIGHT-POINTING ANGLE QUOTATION
                            dc->DrawTextW(L"\u203A", 1u,
                                          chev_fmt_.Get(), cr, brush);
                            trailRight = cr.left - row_chev_gap_px_;
                        }

                        if (value_fmt_ && !row.value.empty()) {
                            const auto vc = pal.settingsRowValue;
                            brush->SetColor(D2D1::ColorF(vc.r, vc.g, vc.b,
                                                         vc.a * contentFade));
                            const D2D1_RECT_F vr{
                                padL, rowTop, trailRight, rowBot,
                            };
                            dc->DrawTextW(row.value.c_str(),
                                          static_cast<UINT32>(row.value.size()),
                                          value_fmt_.Get(), vr, brush);
                        }
                    }
                }
                yc = cardBot;
            }

            // Footer paragraph.
            if (footer_fmt_ && !sec.footer.empty()) {
                yc += footer_top_gap_px_;
                const float fh = footer_text_px_ * 1.4f;
                const D2D1_RECT_F fr{cx0 + card_pad_x_px_,
                                     yc,
                                     cx1 - card_pad_x_px_,
                                     yc + fh * 4.0f};
                const auto fc = pal.settingsRowFooter;
                brush->SetColor(D2D1::ColorF(fc.r, fc.g, fc.b,
                                             fc.a * contentFade));
                dc->DrawTextW(sec.footer.c_str(),
                              static_cast<UINT32>(sec.footer.size()),
                              footer_fmt_.Get(), fr, brush);
                yc += fh + footer_bot_gap_px_;
            }

            yc += card_spacing_px_;
        }
    }

    dc->SetTransform(prev);
}

}  // namespace vrtx::ui
