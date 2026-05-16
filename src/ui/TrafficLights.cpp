#include "ui/TrafficLights.h"

namespace vrtx::ui {

namespace {

inline D2D1::ColorF ToD2D(theme::Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

inline float Distance(D2D1_POINT_2F a, D2D1_POINT_2F b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

void TrafficLights::UpdateLayout(UINT dpi) {
    using namespace theme;

    layout_dpi_ = dpi;

    const float diameter = ToPx(kTrafficLightDiameter, dpi);
    const float radius   = diameter * 0.5f;
    const float spacing  = ToPx(kTrafficLightSpacing,  dpi);
    const float insetX   = ToPx(kTrafficLightInsetX,   dpi) + x_shift_px_;
    const float captionH = ToPx(kCaptionHeight,        dpi);

    // Vertically centre the disc row inside the caption strip, plus
    // any caller-supplied Y-shift (used by the Settings sheet to
    // float the lights down into the sidebar pill instead of hugging
    // the pill's top edge).
    const float centerY = captionH * 0.5f + y_shift_px_;

    const auto& pal = ActivePalette();

    // Order: close, minimize, maximize, left to right.
    const TrafficAction actions[3]{
        TrafficAction::Close,
        TrafficAction::Minimize,
        TrafficAction::Maximize,
    };
    const theme::Color colors[3]{
        pal.tlClose, pal.tlMinimize, pal.tlMaximize,
    };

    for (int i = 0; i < 3; ++i) {
        const float cx = insetX + radius + i * (diameter + spacing);
        discs_[i] = Disc{
            .center = D2D1::Point2F(cx, centerY),
            .radius = radius,
            .action = actions[i],
            .color  = colors[i],
        };
    }

    group_bounds_ = D2D1::RectF(
        discs_.front().center.x - radius - 4,
        centerY - radius - 4,
        discs_.back().center.x  + radius + 4,
        centerY + radius + 4);
}

bool TrafficLights::ContainsAnyDisc(int x, int y) const {
    for (const auto& d : discs_) {
        if (Distance(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)),
                     d.center) <= d.radius) {
            return true;
        }
    }
    return false;
}

void TrafficLights::SetXShift(float pxOffset) {
    if (std::abs(x_shift_px_ - pxOffset) < 0.5f) return;
    x_shift_px_ = pxOffset;
    // Re-layout against the cached DPI; UpdateLayout() reads x_shift_px_
    // when computing insetX, so this picks up the new offset.
    UpdateLayout(layout_dpi_);
}

void TrafficLights::SetYShift(float pxOffset) {
    if (std::abs(y_shift_px_ - pxOffset) < 0.5f) return;
    y_shift_px_ = pxOffset;
    UpdateLayout(layout_dpi_);
}

const TrafficLights::Disc* TrafficLights::DiscAt(int x, int y) const {
    for (const auto& d : discs_) {
        if (Distance(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)),
                     d.center) <= d.radius) {
            return &d;
        }
    }
    return nullptr;
}

TrafficAction TrafficLights::HitTest(int x, int y) const {
    if (const Disc* d = DiscAt(x, y)) return d->action;
    return TrafficAction::None;
}

void TrafficLights::OnMouseMove(int x, int y) {
    // Group-hover region is a slightly inflated bounding rect, so the user
    // doesn't lose the "hover" look while crossing the gap between two discs.
    const bool inGroup =
        x >= group_bounds_.left  && x <= group_bounds_.right &&
        y >= group_bounds_.top   && y <= group_bounds_.bottom;
    group_hovered_ = inGroup;
}

void TrafficLights::OnMouseLeave() {
    group_hovered_ = false;
    pressed_       = TrafficAction::None;
}

void TrafficLights::OnLButtonDown(int x, int y) {
    pressed_ = HitTest(x, y);
}

TrafficAction TrafficLights::OnLButtonUp(int x, int y) {
    const TrafficAction released = HitTest(x, y);
    const TrafficAction firing =
        (pressed_ != TrafficAction::None && pressed_ == released)
            ? released
            : TrafficAction::None;
    pressed_ = TrafficAction::None;
    return firing;
}

void TrafficLights::Render(ID2D1DeviceContext* dc, ID2D1SolidColorBrush* brush,
                           ID2D1Factory* factory, bool windowActive) const {
    const auto& pal = theme::ActivePalette();

    for (const auto& d : discs_) {
        // Inactive window mutes the colors to a uniform gray.
        const theme::Color fill = windowActive ? d.color : pal.tlInactive;
        brush->SetColor(ToD2D(fill));

        D2D1_ELLIPSE ellipse = D2D1::Ellipse(d.center, d.radius, d.radius);
        dc->FillEllipse(ellipse, brush);

        // Subtle rim shadow for depth (1px darker stroke).
        theme::Color rim = fill;
        rim.r *= 0.78f; rim.g *= 0.78f; rim.b *= 0.78f;
        brush->SetColor(ToD2D(rim));
        dc->DrawEllipse(ellipse, brush, 1.0f);

        // Hover glyphs.
        if (group_hovered_ && windowActive) {
            DrawGlyph(dc, brush, factory, d);
        }
    }
}

void TrafficLights::DrawGlyph(ID2D1DeviceContext* dc,
                              ID2D1SolidColorBrush* brush,
                              ID2D1Factory* factory,
                              const Disc& d) const {
    brush->SetColor(ToD2D(theme::ActivePalette().tlGlyph));

    const float r       = d.radius;
    const float thickness = std::max(1.0f, r * 0.22f);

    switch (d.action) {
        case TrafficAction::Close: {
            // ×: two diagonal lines.
            const float off = r * 0.40f;
            const D2D1_POINT_2F p1{d.center.x - off, d.center.y - off};
            const D2D1_POINT_2F p2{d.center.x + off, d.center.y + off};
            const D2D1_POINT_2F p3{d.center.x - off, d.center.y + off};
            const D2D1_POINT_2F p4{d.center.x + off, d.center.y - off};
            dc->DrawLine(p1, p2, brush, thickness);
            dc->DrawLine(p3, p4, brush, thickness);
            break;
        }
        case TrafficAction::Minimize: {
            // −: single horizontal bar.
            const float off = r * 0.50f;
            const D2D1_POINT_2F p1{d.center.x - off, d.center.y};
            const D2D1_POINT_2F p2{d.center.x + off, d.center.y};
            dc->DrawLine(p1, p2, brush, thickness);
            break;
        }
        case TrafficAction::Maximize: {
            // Two filled triangles (two filled triangles, fullscreen-arrow style), drawn
            // through a path geometry.
            ComPtr<ID2D1PathGeometry> path;
            if (FAILED(factory->CreatePathGeometry(path.GetAddressOf()))) break;

            ComPtr<ID2D1GeometrySink> sink;
            if (FAILED(path->Open(sink.GetAddressOf()))) break;

            const float off = r * 0.50f;

            // Top-left triangle.
            sink->BeginFigure(D2D1::Point2F(d.center.x - off, d.center.y - off),
                              D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(d.center.x - off, d.center.y + off * 0.2f));
            sink->AddLine(D2D1::Point2F(d.center.x + off * 0.2f, d.center.y - off));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);

            // Bottom-right triangle.
            sink->BeginFigure(D2D1::Point2F(d.center.x + off, d.center.y + off),
                              D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(d.center.x + off, d.center.y - off * 0.2f));
            sink->AddLine(D2D1::Point2F(d.center.x - off * 0.2f, d.center.y + off));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);

            sink->Close();
            dc->FillGeometry(path.Get(), brush);
            break;
        }
        case TrafficAction::None:
            break;
    }
}

}  // namespace vrtx::ui
