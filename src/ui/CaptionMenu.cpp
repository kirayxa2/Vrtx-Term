#include "ui/CaptionMenu.h"

namespace mactw::ui {

namespace {

inline D2D1::ColorF ToD2D(theme::Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

// Apple-style cubic-out easing. Output ramps quickly at the start and
// settles smoothly into 1, which produces the "soft landing" feel used
// across macOS Tahoe popovers.
float EaseOutCubic(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float Smoothstep(float lo, float hi, float x) {
    const float t = std::clamp((x - lo) / std::max(1e-6f, hi - lo), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

// ---------------------------------------------------------------------------

void CaptionMenu::UpdateLayout(D2D1_RECT_F anchor,
                               int /*squircleWidthPx*/,
                               UINT dpi) {
    using namespace theme;

    row_h_px_       = ToPx(kCaptionMenuRowHeight,    dpi);
    padding_x_px_   = ToPx(kCaptionMenuPaddingX,     dpi);
    padding_y_px_   = ToPx(kCaptionMenuPaddingY,     dpi);
    row_padding_x_px_ = ToPx(kCaptionMenuRowPaddingX, dpi);
    gap_px_         = ToPx(kCaptionMenuGap,          dpi);
    icon_size_px_   = ToPx(kCaptionMenuIconSize,     dpi);
    icon_gap_px_    = ToPx(kCaptionMenuIconGap,      dpi);
    text_size_px_   = ToPx(kCaptionMenuTextSize,     dpi);
    radius_px_      = ToPx(kCaptionMenuCornerRadius, dpi);
    row_radius_px_  = ToPx(kCaptionMenuRowRadius,    dpi);

    const float w = ToPx(kCaptionMenuWidth, dpi);
    const float h = padding_y_px_ * 2.0f
                  + static_cast<float>(items_.size()) * row_h_px_
                  + std::max(0.0f, static_cast<float>(items_.size()) - 1.0f) * gap_px_;

    // Right edge of the panel aligns with the right edge of the caption
    // button. Top edge is anchored just below the button with a small
    // breathing gap.
    const float gapBelow = ToPx(kCaptionMenuAnchorGap, dpi);
    const float right = anchor.right;
    const float left  = right - w;
    const float top   = anchor.bottom + gapBelow;
    const float bot   = top + h;

    bounds_ = D2D1::RectF(left, top, right, bot);
}

bool CaptionMenu::HitTest(int x, int y) const {
    if (!open_ || progress_ <= 0.05f) return false;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= bounds_.left && fx < bounds_.right &&
           fy >= bounds_.top  && fy < bounds_.bottom;
}

float CaptionMenu::ItemTopY(int index) const {
    return bounds_.top + padding_y_px_
         + static_cast<float>(index) * (row_h_px_ + gap_px_);
}

int CaptionMenu::ItemIndexAt(int x, int y) const {
    if (!HitTest(x, y)) return -1;
    const float fy = static_cast<float>(y);
    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = ItemTopY(static_cast<int>(i));
        const float bot = top + row_h_px_;
        if (fy >= top && fy < bot) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CaptionMenu::OnMouseMove(int x, int y) {
    // Suppress hover during the first portion of the open animation so
    // the highlight doesn't chase a moving panel.
    if (progress_ < 0.3f) {
        hover_index_ = -1;
        return;
    }
    hover_index_ = ItemIndexAt(x, y);
}

void CaptionMenu::OnMouseLeave() {
    hover_index_ = -1;
    pressed_index_ = -1;
}

void CaptionMenu::OnLButtonDown(int x, int y) {
    pressed_index_ = ItemIndexAt(x, y);
}

bool CaptionMenu::OnLButtonUp(int x, int y) {
    const int up = ItemIndexAt(x, y);
    const int firedIdx = (pressed_index_ >= 0 && pressed_index_ == up)
                          ? pressed_index_ : -1;
    pressed_index_ = -1;
    if (firedIdx >= 0 && firedIdx < static_cast<int>(items_.size())
        && items_[firedIdx].on_pick) {
        items_[firedIdx].on_pick();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

void CaptionMenu::Render(ID2D1DeviceContext* dc,
                         ID2D1SolidColorBrush* brush,
                         ID2D1Factory* /*factory*/,
                         IDWriteFactory* dwrite) const {
    if (!open_ && progress_ <= 0.0f) return;
    if (items_.empty()) return;

    const auto& pal = theme::ActivePalette();

    const float progress01 = std::clamp(progress_, 0.0f, 1.0f);
    const float ease = EaseOutCubic(progress01);

    // Animate height + opacity. We keep the width fixed (Tahoe popovers
    // reveal vertically only) and only shrink-from-top by 15% so the
    // motion is subtle - more like a fade than a bloom.
    const float fullW = bounds_.right - bounds_.left;
    const float fullH = bounds_.bottom - bounds_.top;
    const float curH  = fullH * (0.85f + 0.15f * ease);
    const float opacity = ease;

    const D2D1_RECT_F rect{
        bounds_.left,
        bounds_.top,
        bounds_.left + fullW,
        bounds_.top  + curH,
    };

    // ---- Drop shadow ---------------------------------------------------
    //
    // Three offset rounded rects with decreasing alpha. Cheaper than a
    // real Gaussian blur per frame and indistinguishable from one at this
    // scale. Apple uses a softer y=8pt blur=20pt under toolbar popovers.
    {
        struct Halo { float dx; float dy; float spread; float alpha; };
        const Halo halos[] = {
            {0.0f,  8.0f, 6.0f, 0.18f * opacity},
            {0.0f,  4.0f, 3.0f, 0.14f * opacity},
            {0.0f,  1.0f, 1.0f, 0.10f * opacity},
        };
        for (const auto& h : halos) {
            const D2D1_ROUNDED_RECT g{
                D2D1::RectF(rect.left  - h.spread + h.dx,
                            rect.top   - h.spread + h.dy,
                            rect.right + h.spread + h.dx,
                            rect.bottom + h.spread + h.dy),
                radius_px_ + h.spread,
                radius_px_ + h.spread,
            };
            brush->SetColor(D2D1::ColorF(0, 0, 0, h.alpha));
            dc->FillRoundedRectangle(g, brush);
        }
    }

    // ---- Panel base fill (Liquid Glass body) ---------------------------
    //
    // Pure flat fill. No top-highlight strip - that was wrong. Tahoe
    // popovers read as a single semi-translucent slab, the depth comes
    // from the shadow underneath.
    {
        const D2D1_ROUNDED_RECT panel{rect, radius_px_, radius_px_};
        const auto base = pal.captionMenuFill;
        brush->SetColor(D2D1::ColorF(base.r, base.g, base.b, base.a * opacity));
        dc->FillRoundedRectangle(panel, brush);
    }

    // ---- Items ---------------------------------------------------------
    //
    // Items fade in slightly after the panel itself so they don't compete
    // with the panel growth. Each row gets a hover overlay that snaps to
    // the row's full padded width, with a smaller corner radius than the
    // panel so it reads as a child element.
    const float itemFade = Smoothstep(0.3f, 1.0f, progress01);
    if (itemFade <= 0.0f) {
        return;
    }

    // Build / rebuild the text formats only when sizes change. This is
    // the hot path of the animation - rebuilding DWrite objects per
    // frame was the source of the perceived stutter.
    if (dwrite && (!label_fmt_ ||
                   std::abs(fmt_built_at_text_size_ - text_size_px_) > 0.5f)) {
        label_fmt_.Reset();
        dwrite->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            text_size_px_, L"en-us",
            label_fmt_.GetAddressOf());
        if (label_fmt_) {
            label_fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            label_fmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            label_fmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        fmt_built_at_text_size_ = text_size_px_;
    }
    if (dwrite && (!icon_fmt_ ||
                   std::abs(fmt_built_at_icon_size_ - icon_size_px_) > 0.5f)) {
        icon_fmt_.Reset();
        dwrite->CreateTextFormat(
            L"Segoe UI Symbol", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            icon_size_px_, L"en-us",
            icon_fmt_.GetAddressOf());
        if (icon_fmt_) {
            icon_fmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            icon_fmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        fmt_built_at_icon_size_ = icon_size_px_;
    }

    // Item geometry: each row is laid out inside [rect.left+padding_x,
    // rect.right-padding_x] horizontally; icon hugs the leading edge,
    // label starts after icon + gap.
    const float rowLeft  = rect.left  + padding_x_px_;
    const float rowRight = rect.right - padding_x_px_;
    const float iconX    = rowLeft + row_padding_x_px_;
    const float textX    = iconX + icon_size_px_ + icon_gap_px_;

    for (size_t i = 0; i < items_.size(); ++i) {
        const float top = ItemTopY(static_cast<int>(i));
        const float bot = top + row_h_px_;

        // Skip rows that fall outside the animated panel rect (during
        // open animation only the upper rows are visible).
        if (top >= rect.bottom) break;

        // Hover highlight - rounded pill spanning the row.
        if (static_cast<int>(i) == hover_index_) {
            const D2D1_ROUNDED_RECT hi{
                D2D1::RectF(rowLeft, top, rowRight, bot),
                row_radius_px_, row_radius_px_,
            };
            const auto h = pal.captionMenuRowHover;
            brush->SetColor(D2D1::ColorF(h.r, h.g, h.b, h.a * itemFade));
            dc->FillRoundedRectangle(hi, brush);
        }

        // Glyph (icon).
        const auto& it = items_[i];
        const auto txt = pal.captionMenuText;
        brush->SetColor(D2D1::ColorF(txt.r, txt.g, txt.b, txt.a * itemFade));

        if (icon_fmt_ && !it.glyph.empty()) {
            const D2D1_RECT_F iconRect{
                iconX,
                top,
                iconX + icon_size_px_,
                bot,
            };
            dc->DrawTextW(it.glyph.c_str(),
                          static_cast<UINT32>(it.glyph.size()),
                          icon_fmt_.Get(), iconRect, brush);
        }

        if (label_fmt_ && !it.label.empty()) {
            const D2D1_RECT_F textRect{
                textX,
                top,
                rowRight - row_padding_x_px_,
                bot,
            };
            dc->DrawTextW(it.label.c_str(),
                          static_cast<UINT32>(it.label.size()),
                          label_fmt_.Get(), textRect, brush);
        }
    }
}

}  // namespace mactw::ui
