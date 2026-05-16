#include "ui/CaptionButton.h"

namespace mactw::ui {

namespace {

inline D2D1::ColorF ToD2D(theme::Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

}  // namespace

void CaptionButton::UpdateLayout(int squircleWidthPx, UINT dpi) {
    using namespace theme;

    const float d        = ToPx(kCaptionButtonDiameter, dpi);
    const float insetX   = ToPx(kCaptionButtonInsetX,   dpi);
    const float offsetY  = ToPx(kCaptionButtonOffsetY,  dpi);
    const float captionH = ToPx(kCaptionHeight,         dpi);

    // Right-anchored at the squircle's right edge inset by `insetX`,
    // vertically centred on the caption strip with a small downward
    // offset to optically balance the squircle's curving top edge.
    const float right = static_cast<float>(squircleWidthPx) - insetX;
    const float left  = right - d;
    const float cy    = captionH * 0.5f + offsetY;
    const float top   = cy - d * 0.5f;
    const float bot   = cy + d * 0.5f;

    bounds_ = D2D1::RectF(left, top, right, bot);
}

bool CaptionButton::HitTest(int x, int y) const {
    const float cx = (bounds_.left + bounds_.right)  * 0.5f;
    const float cy = (bounds_.top  + bounds_.bottom) * 0.5f;
    const float r  = (bounds_.right - bounds_.left)  * 0.5f;
    const float dx = static_cast<float>(x) - cx;
    const float dy = static_cast<float>(y) - cy;
    return (dx * dx + dy * dy) <= (r * r);
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

    const float cx = (bounds_.left + bounds_.right)  * 0.5f;
    const float cy = (bounds_.top  + bounds_.bottom) * 0.5f;
    const float r  = (bounds_.right - bounds_.left)  * 0.5f;

    const D2D1_ELLIPSE disc = D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r);

    // ---- Base fill: always visible -------------------------------------
    if (windowActive) {
        brush->SetColor(ToD2D(pal.captionButtonFill));
    } else {
        const auto c = pal.captionButtonFill;
        brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * 0.5f));
    }
    dc->FillEllipse(disc, brush);

    // While the menu is open the disc keeps a press-strength fill so it
    // visually anchors the popup. Mix expansion into the overlay so the
    // transition is continuous (no pop) when opening / closing.
    if (windowActive) {
        if (expansion_ > 0.0f) {
            const auto c = pal.captionButtonPressed;
            brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * expansion_));
            dc->FillEllipse(disc, brush);
        } else if (pressed_) {
            brush->SetColor(ToD2D(pal.captionButtonPressed));
            dc->FillEllipse(disc, brush);
        } else if (hovered_) {
            brush->SetColor(ToD2D(pal.captionButtonHover));
            dc->FillEllipse(disc, brush);
        }
    }

    // ---- Hairline outline ----------------------------------------------
    const float dpiFactor   = r / std::max(1.0f, theme::kCaptionButtonDiameter * 0.5f);
    const float strokeAtDpi = std::max(1.0f,
                                       theme::kWindowBorderWidth * dpiFactor);

    const D2D1_ELLIPSE outline = D2D1::Ellipse(
        D2D1::Point2F(cx, cy),
        r - strokeAtDpi * 0.5f,
        r - strokeAtDpi * 0.5f);
    brush->SetColor(ToD2D(pal.windowBorder));
    dc->DrawEllipse(outline, brush, strokeAtDpi);

    // ---- Chevron-down glyph (rotates 180deg as expansion -> 1) ---------
    //
    // Apple's SF Symbol "chevron.down" inside a 28pt button uses a flat,
    // wide V (1.8:1). We draw it as a single polyline so the apex uses
    // the path's lineJoin (round) instead of two overlapping round caps,
    // and rotate it about the disc centre by 180deg * expansion.
    const float diameter = r * 2.0f;
    const float halfW    = diameter * 0.18f;
    const float halfH    = diameter * 0.10f;

    const D2D1_POINT_2F p_left  {cx - halfW, cy - halfH};
    const D2D1_POINT_2F p_tip   {cx,         cy + halfH};
    const D2D1_POINT_2F p_right {cx + halfW, cy - halfH};

    const theme::Color glyphCol = windowActive ? pal.captionButtonGlyph
                                               : pal.tlInactive;
    brush->SetColor(ToD2D(glyphCol));

    const float chevStroke = std::max(1.5f, diameter * 0.07f);

    ComPtr<ID2D1StrokeStyle> ss;
    D2D1_STROKE_STYLE_PROPERTIES props{};
    props.startCap   = D2D1_CAP_STYLE_ROUND;
    props.endCap     = D2D1_CAP_STYLE_ROUND;
    props.lineJoin   = D2D1_LINE_JOIN_ROUND;
    props.miterLimit = 4.0f;
    factory->CreateStrokeStyle(props, nullptr, 0, ss.GetAddressOf());

    ComPtr<ID2D1PathGeometry> path;
    factory->CreatePathGeometry(path.GetAddressOf());
    ComPtr<ID2D1GeometrySink> sink;
    if (SUCCEEDED(path->Open(sink.GetAddressOf()))) {
        sink->BeginFigure(p_left, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLine(p_tip);
        sink->AddLine(p_right);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();

        // Save/restore transform so we don't disturb the caller. The
        // rotation is centred on the disc, not on the chevron's bbox -
        // this keeps the glyph centered through the whole flip.
        D2D1_MATRIX_3X2_F prev;
        dc->GetTransform(&prev);
        const float angleDeg = 180.0f * std::clamp(expansion_, 0.0f, 1.0f);
        const D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(
            angleDeg, D2D1::Point2F(cx, cy));
        dc->SetTransform(rot * prev);

        dc->DrawGeometry(path.Get(), brush, chevStroke, ss.Get());

        dc->SetTransform(prev);
    }
}

}  // namespace mactw::ui
