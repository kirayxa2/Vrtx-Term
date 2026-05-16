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
    // The button is a perfect circle, so width == height == diameter.
    const float right = static_cast<float>(squircleWidthPx) - insetX;
    const float left  = right - d;
    const float cy    = captionH * 0.5f + offsetY;
    const float top   = cy - d * 0.5f;
    const float bot   = cy + d * 0.5f;

    bounds_ = D2D1::RectF(left, top, right, bot);
}

bool CaptionButton::HitTest(int x, int y) const {
    // Hit-test against the actual circular shape, not the bounding rect,
    // so clicks just outside the disc (in the caption strip corners) fall
    // through to dragging instead of being eaten by the button.
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
    //
    // Apple keeps a faint dark wash under the glyph at all times so the
    // button's shape reads even when the cursor is nowhere near it. Hover
    // and press add an extra translucent overlay on top of this base.
    if (windowActive) {
        brush->SetColor(ToD2D(pal.captionButtonFill));
    } else {
        // Dim the fill on inactive windows so the chrome reads as muted.
        const auto c = pal.captionButtonFill;
        brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a * 0.5f));
    }
    dc->FillEllipse(disc, brush);

    if (windowActive) {
        if (pressed_) {
            brush->SetColor(ToD2D(pal.captionButtonPressed));
            dc->FillEllipse(disc, brush);
        } else if (hovered_) {
            brush->SetColor(ToD2D(pal.captionButtonHover));
            dc->FillEllipse(disc, brush);
        }
    }

    // ---- Hairline outline ----------------------------------------------
    //
    // Same colour and stroke width as the window border, so the button
    // visually belongs to the chrome rather than feeling pasted on. We
    // inset the geometry by half a stroke so the entire stroke is visible
    // (D2D centres strokes on the path).
    //
    // Stroke thickness scales with the disc's actual radius rather than a
    // hard-coded DPI conversion, which keeps the hairline crisp on any
    // monitor without us having to thread `dpi_` into Render().
    const float dpiFactor   = r / std::max(1.0f, theme::kCaptionButtonDiameter * 0.5f);
    const float strokeAtDpi = std::max(1.0f,
                                       theme::kWindowBorderWidth * dpiFactor);

    const D2D1_ELLIPSE outline = D2D1::Ellipse(
        D2D1::Point2F(cx, cy),
        r - strokeAtDpi * 0.5f,
        r - strokeAtDpi * 0.5f);
    brush->SetColor(ToD2D(pal.windowBorder));
    dc->DrawEllipse(outline, brush, strokeAtDpi);

    // ---- Chevron-down glyph --------------------------------------------
    //
    // Apple's SF Symbol "chevron.down" (Regular weight, scale Medium) is
    // a thin V centred horizontally and vertically on the disc.
    //
    // Reverse-engineered proportions from a high-res macOS Tahoe Mail
    // toolbar capture, normalised to a 28pt button:
    //   chevron width  = ~36% of the disc diameter
    //   chevron height = ~20% of the disc diameter
    //   stroke         = ~7%  of the disc diameter
    //
    // The 1.8:1 width-to-height ratio + thin stroke produce the flat,
    // wide, light V that reads as Apple's chevron rather than a generic
    // chunky "v". Geometric centring (midpoint of the bounding box on the
    // disc centre) is correct here - SF Symbols already bakes the visual
    // balance into its glyph metrics.
    const float diameter = r * 2.0f;
    const float halfW    = diameter * 0.18f;   // half of chevron width
    const float halfH    = diameter * 0.10f;   // half of chevron height

    const D2D1_POINT_2F p_left  {cx - halfW, cy - halfH};
    const D2D1_POINT_2F p_tip   {cx,         cy + halfH};
    const D2D1_POINT_2F p_right {cx + halfW, cy - halfH};

    const theme::Color glyphCol = windowActive ? pal.captionButtonGlyph
                                               : pal.tlInactive;
    brush->SetColor(ToD2D(glyphCol));

    const float chevStroke = std::max(1.5f, diameter * 0.07f);

    // SF Symbols chevrons use round caps + round join. Drawing as a
    // single polyline (not two separate lines) is critical: round-join
    // at the apex gives a smooth point, whereas two independent
    // `DrawLine` calls would render two overlapping round caps and bake
    // them into a chunky bulge.
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
        dc->DrawGeometry(path.Get(), brush, chevStroke, ss.Get());
    }
}

}  // namespace mactw::ui
