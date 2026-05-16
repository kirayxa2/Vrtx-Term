#include "ui/CaptionButton.h"

namespace mactw::ui {

namespace {

inline D2D1::ColorF ToD2D(theme::Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

}  // namespace

void CaptionButton::UpdateLayout(int squircleWidthPx, UINT dpi) {
    using namespace theme;

    const float w        = ToPx(kCaptionButtonWidth,    dpi);
    const float h        = ToPx(kCaptionButtonHeight,   dpi);
    const float insetX   = ToPx(kCaptionButtonInsetX,   dpi);
    const float captionH = ToPx(kCaptionHeight,         dpi);

    // Vertically centre inside the caption strip; right-anchored at the
    // squircle's right edge inset by `insetX`. Mirrors the traffic-light
    // group's left inset, so the chrome reads as symmetric.
    const float right = static_cast<float>(squircleWidthPx) - insetX;
    const float left  = right - w;
    const float cy    = captionH * 0.5f;
    const float top   = cy - h * 0.5f;
    const float bot   = cy + h * 0.5f;

    bounds_ = D2D1::RectF(left, top, right, bot);
}

bool CaptionButton::HitTest(int x, int y) const {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= bounds_.left && fx < bounds_.right &&
           fy >= bounds_.top  && fy < bounds_.bottom;
}

void CaptionButton::OnMouseMove(int x, int y) {
    hovered_ = HitTest(x, y);
}

void CaptionButton::OnMouseLeave() {
    hovered_ = false;
    pressed_ = false;
}

void CaptionButton::OnLButtonDown(int x, int y) {
    pressed_ = HitTest(x, y);
}

bool CaptionButton::OnLButtonUp(int x, int y) {
    const bool inside = HitTest(x, y);
    const bool fire   = pressed_ && inside;
    pressed_ = false;
    if (fire && on_click_) {
        on_click_();
    }
    return fire;
}

// ---------------------------------------------------------------------------

void CaptionButton::Render(ID2D1DeviceContext* dc,
                           ID2D1SolidColorBrush* brush,
                           ID2D1Factory* factory,
                           bool windowActive) const {
    const auto& pal = theme::ActivePalette();

    // Pill background: invisible until hover (or press). The radius is
    // half the height so the short ends are perfectly rounded.
    const float radius = (bounds_.bottom - bounds_.top) * 0.5f;
    const D2D1_ROUNDED_RECT pill{bounds_, radius, radius};

    if (windowActive) {
        if (pressed_) {
            brush->SetColor(ToD2D(pal.captionButtonPressed));
            dc->FillRoundedRectangle(pill, brush);
        } else if (hovered_) {
            brush->SetColor(ToD2D(pal.captionButtonHover));
            dc->FillRoundedRectangle(pill, brush);
        }
    }

    // Chevron-down: two strokes that meet at the bottom centre, opening
    // upwards in a "v" with a gentle 22deg slope (Apple's chevron uses
    // ~25deg; close enough at our cell size). The chevron sits a touch
    // above the pill's vertical centre to keep it optically balanced.
    const float cx = (bounds_.left + bounds_.right) * 0.5f;
    const float cy = (bounds_.top  + bounds_.bottom) * 0.5f;

    // Chevron is 60% of the pill width, 30% of the pill height.
    const float chevW = (bounds_.right - bounds_.left) * 0.30f;
    const float chevH = (bounds_.bottom - bounds_.top) * 0.18f;

    const D2D1_POINT_2F p_left  {cx - chevW, cy - chevH * 0.5f};
    const D2D1_POINT_2F p_tip   {cx,         cy + chevH * 0.5f};
    const D2D1_POINT_2F p_right {cx + chevW, cy - chevH * 0.5f};

    // The active-window glyph is full-strength; an inactive window mutes
    // it like the traffic-lights inactive disc. Bold-but-not-blocky weight
    // so it reads at small sizes.
    const theme::Color glyphCol = windowActive ? pal.captionButtonGlyph
                                               : pal.tlInactive;
    brush->SetColor(ToD2D(glyphCol));

    const float strokePx = std::max(1.0f, chevH * 0.45f);

    // Use a stroke style with rounded caps + miter join so the apex looks
    // continuous instead of having a tiny gap at the seam.
    ComPtr<ID2D1StrokeStyle> ss;
    D2D1_STROKE_STYLE_PROPERTIES props{};
    props.startCap   = D2D1_CAP_STYLE_ROUND;
    props.endCap     = D2D1_CAP_STYLE_ROUND;
    props.lineJoin   = D2D1_LINE_JOIN_ROUND;
    props.miterLimit = 4.0f;
    factory->CreateStrokeStyle(props, nullptr, 0, ss.GetAddressOf());

    dc->DrawLine(p_left,  p_tip,   brush, strokePx, ss.Get());
    dc->DrawLine(p_tip,   p_right, brush, strokePx, ss.Get());
}

}  // namespace mactw::ui
