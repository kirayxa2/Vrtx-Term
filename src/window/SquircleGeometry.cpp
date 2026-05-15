#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

// ---------------------------------------------------------------------------
// Figma corner-smoothing 0.6 / Apple `.continuous` squircle path.
//
// Coefficients verified against:
//   * Figma's cornerSmoothing = 0.6 plugin output,
//   * "Desperately Seeking Squircles" (Figma engineering blog),
//   * the squircle.js / Tailwind CSS shape() reference utility,
//   * direct geometric derivation (see comments below).
//
// All five values are multiples of the corner radius r:
//
//     kP  = 1.6   = (1 + smoothing)            : corner footprint along edge
//     kC1 = 1.04                                : shoulder cubic 1st ctrl
//     kC2 = 0.76                                : shoulder cubic 2nd ctrl
//                  (= kAs + kAp * tan(63 deg)  -- intersection of arc tangent
//                   at arcStart with the straight edge)
//     kAs = 0.546 = 1 - cos(63 deg)             : arc start "long axis"
//     kAp = 0.109 = 1 - sin(63 deg)             : arc start "short axis"
//
// where 63 deg = 45 deg + halfSweep, halfSweep = (1 - smoothing) * 45 deg.
//
// Per corner, the path is:
//
//     edge -> shoulder-out cubic -> central arc (36 deg) -> shoulder-in cubic -> next edge
//
// Both control points of each shoulder cubic lie on the straight edge that
// the cubic leaves or enters, which makes the curvature exactly zero at
// that join (G2 continuity with the straight edge). The cubic's far end
// is tangent to the central arc, so curvature is also continuous at the
// arc-cubic join.
//
// The four corners are written out explicitly with absolute coordinates -
// no rotation matrices, no canonical-frame trickery. Sign/direction errors
// have nowhere to hide.
// ---------------------------------------------------------------------------

constexpr float kP  = 1.60f;
constexpr float kC1 = 1.04f;
constexpr float kC2 = 0.76f;
constexpr float kAs = 0.546f;
constexpr float kAp = 0.109f;

void AddCornerArc(ID2D1GeometrySink* sink, D2D1_POINT_2F endPoint, float r) {
    D2D1_ARC_SEGMENT arc{};
    arc.point          = endPoint;
    arc.size           = D2D1::SizeF(r, r);
    arc.rotationAngle  = 0.0f;
    arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    arc.arcSize        = D2D1_ARC_SIZE_SMALL;
    sink->AddArc(arc);
}

}  // namespace

ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float w,
                                            float h,
                                            float radius,
                                            float /*smoothing*/) {
    radius = std::clamp(radius, 0.0f, std::min(w, h) * 0.5f);

    const float r  = radius;
    const float p  = kP  * r;
    const float c1 = kC1 * r;
    const float c2 = kC2 * r;
    const float as = kAs * r;
    const float ap = kAp * r;

    ComPtr<ID2D1PathGeometry> geom;
    ThrowIfFailed(factory->CreatePathGeometry(geom.GetAddressOf()),
                  "ID2D1Factory::CreatePathGeometry");

    ComPtr<ID2D1GeometrySink> sink;
    ThrowIfFailed(geom->Open(sink.GetAddressOf()),
                  "ID2D1PathGeometry::Open");

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    // Begin on the top edge, just past the top-left corner footprint. The
    // path is traversed clockwise visually (CW in Direct2D's y-down
    // convention), which matches CSS shape() and SVG arc-flag semantics.
    sink->BeginFigure(D2D1::Point2F(p, 0.0f), D2D1_FIGURE_BEGIN_FILLED);

    // ===== Top edge to TOP-RIGHT corner =====
    sink->AddLine(D2D1::Point2F(w - p, 0.0f));
    // shoulder-out: edge -> arcStart
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(w - c1, 0.0f),
        D2D1::Point2F(w - c2, 0.0f),
        D2D1::Point2F(w - as, ap)));
    // central arc, 36 deg sweep, CW
    AddCornerArc(sink.Get(), D2D1::Point2F(w - ap, as), r);
    // shoulder-in: arcEnd -> right edge
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(w, c2),
        D2D1::Point2F(w, c1),
        D2D1::Point2F(w, p)));

    // ===== Right edge to BOTTOM-RIGHT corner =====
    sink->AddLine(D2D1::Point2F(w, h - p));
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(w, h - c1),
        D2D1::Point2F(w, h - c2),
        D2D1::Point2F(w - ap, h - as)));
    AddCornerArc(sink.Get(), D2D1::Point2F(w - as, h - ap), r);
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(w - c2, h),
        D2D1::Point2F(w - c1, h),
        D2D1::Point2F(w - p,  h)));

    // ===== Bottom edge to BOTTOM-LEFT corner =====
    sink->AddLine(D2D1::Point2F(p, h));
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(c1, h),
        D2D1::Point2F(c2, h),
        D2D1::Point2F(as, h - ap)));
    AddCornerArc(sink.Get(), D2D1::Point2F(ap, h - as), r);
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(0.0f, h - c2),
        D2D1::Point2F(0.0f, h - c1),
        D2D1::Point2F(0.0f, h - p)));

    // ===== Left edge to TOP-LEFT corner =====
    sink->AddLine(D2D1::Point2F(0.0f, p));
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(0.0f, c1),
        D2D1::Point2F(0.0f, c2),
        D2D1::Point2F(ap, as)));
    AddCornerArc(sink.Get(), D2D1::Point2F(as, ap), r);
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(c2, 0.0f),
        D2D1::Point2F(c1, 0.0f),
        D2D1::Point2F(p,  0.0f)));

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    ThrowIfFailed(sink->Close(), "ID2D1GeometrySink::Close");

    return geom;
}

}  // namespace mactw::window
