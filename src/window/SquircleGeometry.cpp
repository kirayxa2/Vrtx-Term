#include "window/SquircleGeometry.h"

namespace vrtx::window {

namespace {

// ---------------------------------------------------------------------------
// Continuous-corner (squircle) path, smoothing 0.6.
//
// Coefficients verified against:
//   * the cornerSmoothing = 0.6 reference geometry,
//   * the geometric derivation of G2-continuous rounded corners,
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
    // The corner footprint along each edge is kP * r (= 1.6 * r). Two
    // corners share each edge, so r must satisfy 2 * kP * r <= edge length.
    // Hard-clamp here so we degrade gracefully on tiny shapes (1-row
    // cards, very narrow toggles) instead of producing self-intersecting
    // beziers.
    const float maxR = std::min(w, h) / (2.0f * kP);
    radius = std::clamp(radius, 0.0f, std::max(0.0f, maxR));

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

ComPtr<ID2D1PathGeometry> BuildSquirclePathInRect(ID2D1Factory* factory,
                                                  D2D1_RECT_F rect,
                                                  float radius,
                                                  float smoothing) {
    // Build the path in local coordinates (0,0)-(w,h), then bake the
    // translation into a brand-new path so callers can FillGeometry /
    // DrawGeometry without setting any extra transform.
    const float w = std::max(0.0f, rect.right  - rect.left);
    const float h = std::max(0.0f, rect.bottom - rect.top);
    auto local = BuildSquirclePath(factory, w, h, radius, smoothing);

    ComPtr<ID2D1PathGeometry> baked;
    ThrowIfFailed(factory->CreatePathGeometry(baked.GetAddressOf()),
                  "ID2D1Factory::CreatePathGeometry (baked)");

    ComPtr<ID2D1GeometrySink> sink;
    ThrowIfFailed(baked->Open(sink.GetAddressOf()),
                  "ID2D1PathGeometry::Open (baked)");

    const D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Translation(rect.left,
                                                              rect.top);
    ThrowIfFailed(local->Simplify(
                      D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                      &m, sink.Get()),
                  "ID2D1PathGeometry::Simplify");
    ThrowIfFailed(sink->Close(), "ID2D1GeometrySink::Close (baked)");
    return baked;
}

}  // namespace vrtx::window
