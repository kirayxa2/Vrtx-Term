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

    outer_pad_px_       = ToPx(kSettingsOuterPadding,     dpi);
    pane_radius_px_     = ToPx(kSettingsPaneCornerRadius, dpi);
    row_h_px_           = ToPx(kSettingsRowHeight,        dpi);
    row_gap_px_         = ToPx(kSettingsRowGap,           dpi);
    row_pad_x_px_       = ToPx(kSettingsRowPaddingX,      dpi);
    row_radius_px_      = ToPx(kSettingsRowRadius,        dpi);
    icon_size_px_       = ToPx(kSettingsRowIconSize,      dpi);
    icon_gap_px_        = ToPx(kSettingsRowIconGap,       dpi);
    row_text_px_        = ToPx(kSettingsRowTextSize,      dpi);
    header_text_px_     = ToPx(kSettingsHeaderTextSize,   dpi);
    header_pad_x_px_    = ToPx(kSettingsHeaderPaddingX,   dpi);
    header_top_gap_px_  = ToPx(kSettingsHeaderTopGap,     dpi);
    header_bot_gap_px_  = ToPx(kSettingsHeaderBottomGap,  dpi);
    content_pad_x_px_   = ToPx(kSettingsContentPaddingX,  dpi);
    content_pad_y_px_   = ToPx(kSettingsContentPaddingY,  dpi);
    title_text_px_      = ToPx(kSettingsTitleSize,        dpi);
    body_text_px_       = ToPx(kSettingsBodyTextSize,     dpi);
    close_diam_px_      = ToPx(kSettingsCloseDiameter,    dpi);

    const float sbW = ToPx(kSettingsSidebarWidth, dpi);

    // Inset under caption strip.
    const float top    = squircleRect.top + captionHeightPx
                       + outer_pad_px_;
    const float bot    = squircleRect.bottom - outer_pad_px_;
    const float left   = squircleRect.left   + outer_pad_px_;
    const float right  = squircleRect.right  - outer_pad_px_;

    sidebar_.left   = left;
    sidebar_.top    = top;
    sidebar_.right  = left + sbW;
    sidebar_.bottom = bot;

    content_.left   = sidebar_.right + outer_pad_px_;
    content_.top    = top;
    content_.right  = right;
    content_.bottom = bot;

    // Close-X button: small disc in top-right of sidebar.
    const float cInsetX = ToPx(kSettingsCloseInsetX, dpi);
    const float cInsetY = ToPx(kSettingsCloseInsetY, dpi);
    close_btn_.right  = sidebar_.right - cInsetX;
    close_btn_.left   = close_btn_.right - close_diam_px_;
    close_btn_.top    = sidebar_.top + cInsetY;
    close_btn_.bottom = close_btn_.top + close_diam_px_;
}

bool SettingsView::HitTestPanel(int x, int y) const {
    if (!open_ || progress_ <= 0.05f) return false;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const bool inSidebar = fx >= sidebar_.left && fx < sidebar_.right
                        && fy >= sidebar_.top  && fy < sidebar_.bottom;
    const bool inContent = fx >= content_.left && fx < content_.right
                        && fy >= content_.top  && fy < content_.bottom;
    return inSidebar || inContent;
}

bool SettingsView::CloseAt(int x, int y) const {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= close_btn_.left && fx < close_btn_.right &&
           fy >= close_btn_.top  && fy < close_btn_.bottom;
}

int SettingsView::RowAt(int x, int y) const {
    if (items_.empty()) return -1;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    if (fx < sidebar_.left || fx >= sidebar_.right) return -1;

    // First row sits below the close button + a small breathing gap so
    // the X never overlaps with row hover/active fills.
    const float rowsTop = close_btn_.bottom + header_top_gap_px_;
    if (fy < rowsTop) return -1;

    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = rowsTop + static_cast<float>(i)
                          * (row_h_px_ + row_gap_px_);
        const float bot = top + row_h_px_;
        if (fy >= top && fy < bot) return static_cast<int>(i);
    }
    return -1;
}

void SettingsView::OnMouseMove(int x, int y) {
    if (progress_ < 0.4f) {
        hover_index_ = -1;
        hover_close_ = false;
        return;
    }
    hover_close_ = CloseAt(x, y);
    hover_index_ = hover_close_ ? -1 : RowAt(x, y);
}

void SettingsView::OnMouseLeave() {
    hover_index_   = -1;
    pressed_index_ = -1;
    hover_close_   = false;
    pressed_close_ = false;
}

void SettingsView::OnLButtonDown(int x, int y) {
    pressed_close_ = CloseAt(x, y);
    pressed_index_ = pressed_close_ ? -1 : RowAt(x, y);
}

bool SettingsView::OnLButtonUp(int x, int y) {
    const bool fireClose = pressed_close_ && CloseAt(x, y);
    const int  upRow     = pressed_close_ ? -1 : RowAt(x, y);
    const int  fireRow   = (pressed_index_ >= 0 && pressed_index_ == upRow)
                            ? pressed_index_ : -1;
    pressed_close_ = false;
    pressed_index_ = -1;

    if (fireRow >= 0 && fireRow < static_cast<int>(items_.size())) {
        active_index_ = fireRow;
        if (items_[fireRow].on_pick) items_[fireRow].on_pick();
    }
    return fireClose;
}

// ---------------------------------------------------------------------------

void SettingsView::Render(ID2D1DeviceContext* dc,
                          ID2D1SolidColorBrush* brush,
                          ID2D1Factory* /*factory*/,
                          IDWriteFactory* dwrite) const {
    if (!open_ && progress_ <= 0.0f) return;

    const auto& pal = theme::ActivePalette();
    const float ease = EaseOutCubic(progress_);
    const float fade = ease;

    // ---- Scrim over the terminal area (caption strip stays clean) -----
    {
        const D2D1_RECT_F scrim{
            sidebar_.left  - outer_pad_px_,                 // squircle left edge
            sidebar_.top   - outer_pad_px_,                 // top of caption-bottom
            content_.right + outer_pad_px_,                 // squircle right edge
            content_.bottom + outer_pad_px_,                // squircle bottom
        };
        const auto sc = pal.settingsScrim;
        brush->SetColor(D2D1::ColorF(sc.r, sc.g, sc.b, sc.a * fade));
        dc->FillRectangle(scrim, brush);
    }

    // Slight slide-from-bottom + fade. Tahoe sheets drop a few px;
    // we mirror by translating the panes upward as the animation lands.
    const float slidePx = (1.0f - ease) * 14.0f;

    D2D1_MATRIX_3X2_F prev;
    dc->GetTransform(&prev);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, slidePx) * prev);

    // ---- Sidebar pane --------------------------------------------------
    {
        const D2D1_ROUNDED_RECT panel{sidebar_,
                                      pane_radius_px_, pane_radius_px_};
        const auto fill = pal.settingsSidebarFill;
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillRoundedRectangle(panel, brush);
    }

    // ---- Content pane --------------------------------------------------
    {
        const D2D1_ROUNDED_RECT panel{content_,
                                      pane_radius_px_, pane_radius_px_};
        const auto fill = pal.settingsContentFill;
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillRoundedRectangle(panel, brush);
    }

    // ---- Close-X button -----------------------------------------------
    {
        const float cx = (close_btn_.left + close_btn_.right) * 0.5f;
        const float cy = (close_btn_.top  + close_btn_.bottom) * 0.5f;
        const float r  = close_diam_px_ * 0.5f;

        // Disc with a pressure-feedback alpha bump on hover/press.
        const D2D1_ELLIPSE disc{D2D1::Point2F(cx, cy), r, r};
        auto fill = pal.settingsCloseFill;
        if (pressed_close_) fill.a = std::min(1.0f, fill.a * 2.4f);
        else if (hover_close_) fill.a = std::min(1.0f, fill.a * 1.6f);
        brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b, fill.a * fade));
        dc->FillEllipse(disc, brush);

        // Two crossed strokes (X). Apple uses xmark.circle.fill in
        // SF Symbols, but the stroked version reads better at 22pt.
        const float arm = r * 0.42f;
        const auto g = pal.settingsCloseGlyph;
        brush->SetColor(D2D1::ColorF(g.r, g.g, g.b, g.a * fade));
        const float stroke = std::max(1.5f, r * 0.18f);
        dc->DrawLine(D2D1::Point2F(cx - arm, cy - arm),
                     D2D1::Point2F(cx + arm, cy + arm),
                     brush, stroke);
        dc->DrawLine(D2D1::Point2F(cx + arm, cy - arm),
                     D2D1::Point2F(cx - arm, cy + arm),
                     brush, stroke);
    }

    // ---- Content fades in slightly behind the panels so the sheet
    //      reads as "container first, contents after". ----
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

    EnsureFormat(dwrite, icon_fmt_,   built_at_icon_,
                 L"Segoe UI Symbol", icon_size_px_,
                 DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (icon_fmt_) icon_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    EnsureFormat(dwrite, header_fmt_, built_at_header_,
                 L"Segoe UI", header_text_px_,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (header_fmt_) header_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

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

    // ---- Sidebar rows --------------------------------------------------
    const float rowsTop = close_btn_.bottom + header_top_gap_px_;
    const float rowLeft  = sidebar_.left  + row_pad_x_px_;
    const float rowRight = sidebar_.right - row_pad_x_px_;
    const float iconX    = rowLeft + row_pad_x_px_;
    const float textX    = iconX + icon_size_px_ + icon_gap_px_;

    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = rowsTop
                        + static_cast<float>(i) * (row_h_px_ + row_gap_px_);
        const float bot = top + row_h_px_;
        if (top >= sidebar_.bottom) break;

        const bool active  = (static_cast<int>(i) == active_index_);
        const bool hovered = (static_cast<int>(i) == hover_index_);

        // Row pill: active = accent, hover = subtle overlay.
        if (active || hovered) {
            const D2D1_ROUNDED_RECT pill{
                D2D1::RectF(rowLeft, top, rowRight, bot),
                row_radius_px_, row_radius_px_,
            };
            const auto c = active ? pal.settingsRowActive
                                  : pal.settingsRowHover;
            brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * contentFade));
            dc->FillRoundedRectangle(pill, brush);
        }

        const auto& it = items_[i];
        const auto txtCol = active ? pal.settingsRowTextActive
                                   : pal.settingsRowText;
        brush->SetColor(D2D1::ColorF(txtCol.r, txtCol.g, txtCol.b,
                                     txtCol.a * contentFade));

        if (icon_fmt_ && !it.glyph.empty()) {
            const D2D1_RECT_F iconRect{
                iconX, top, iconX + icon_size_px_, bot
            };
            dc->DrawTextW(it.glyph.c_str(),
                          static_cast<UINT32>(it.glyph.size()),
                          icon_fmt_.Get(), iconRect, brush);
        }
        if (row_fmt_ && !it.label.empty()) {
            const D2D1_RECT_F textRect{
                textX, top, rowRight - row_pad_x_px_, bot
            };
            dc->DrawTextW(it.label.c_str(),
                          static_cast<UINT32>(it.label.size()),
                          row_fmt_.Get(), textRect, brush);
        }
    }

    // ---- Content pane: title + body for the active row ----------------
    if (active_index_ >= 0
        && active_index_ < static_cast<int>(items_.size())) {
        const auto& it = items_[active_index_];

        const float cx0 = content_.left  + content_pad_x_px_;
        const float cy0 = content_.top   + content_pad_y_px_;
        const float cx1 = content_.right - content_pad_x_px_;

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

        if (body_fmt_ && !it.body.empty()) {
            const float bodyTop = cy0 + title_text_px_ * 1.4f
                                + body_text_px_ * 0.6f;
            const D2D1_RECT_F r{
                cx0, bodyTop, cx1, content_.bottom - content_pad_y_px_
            };
            const auto c = pal.settingsBody;
            brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * contentFade));
            dc->DrawTextW(it.body.c_str(),
                          static_cast<UINT32>(it.body.size()),
                          body_fmt_.Get(), r, brush);
        }
    }

    dc->SetTransform(prev);
}

}  // namespace mactw::ui
