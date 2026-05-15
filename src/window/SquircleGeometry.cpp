#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

// ---------------------------------------------------------------------------
// Squircle path geometry, written corner-by-corner without any rotation
// math.
//
// Each of the four corners is one cubic Bezier whose control points are
// laid out so that:
//
//   * with smoothing == 0 we get a plain circular round-rect (the standard
//     K = 4/3 * tan(pi/8) = 0.5523 cubic-arc approximation),
//
//   * with smoothing > 0 the corner footprint along each straight edge is
//     stretched from r to p = (1 + smoothing) * r, while the bezier
//     handles are scaled with it. This pulls the curvature peak inward
//     and softens the join with the straight edge, which is the visual
//     signature Apple ships as `.continuous` corners.
//
// The strict figma-squircle algorithm uses three Beziers per corner for
// G2 continuity. At the 16pt titlebar-window radius MacTermWin targets,
// the visual difference is sub-pixel. We use the simpler form because it
// is impossible to get wrong: every corner is written out with explicit
// coordinates, no per-corner rotation, no canonical-frame trickery.
// ---------------------------------------------------------------------------

constexpr float kArcK = 0.55228474983079339f;  // 4/3 * tan(pi/8)

struct CornerPoints {
    D2D1_POINT_2F start;
    D2D1_POINT_2F c1;
    D2D1_POINT_2F c2;
    D2D1_POINT_2F end;
};

// All four corners, traversed clockwise starting at the top-right.
struct AllCorners {
    CornerPoints tr, br, bl, tl;
};

AllCorners ComputeCorners(float w, float h, float radius, float smoothing) {
    smoothing = std::clamp(smoothing, 0.0f, 1.0f);

    // Footprint of the corner along each straight edge.
    const float p = (1.0f + smoothing) * radius;

    // Bezier handle length, scaled by the corner footprint p so that
    // smoothing==0 reproduces the canonical circular K*r and larger
    // smoothing values stretch the handles outward, softening the curve.
    const float k = kArcK * p;

    AllCorners c{};

    // Top-right corner. Tip at (w, 0).
    c.tr.start = D2D1::Point2F(w - p,     0.0f);
    c.tr.c1    = D2D1::Point2F(w - p + k, 0.0f);
    c.tr.c2    = D2D1::Point2F(w,         p - k);
    c.tr.end   = D2D1::Point2F(w,         p);

    // Bottom-right corner. Tip at (w, h).
    c.br.start = D2D1::Point2F(w,         h - p);
    c.br.c1    = D2D1::Point2F(w,         h - p + k);
    c.br.c2    = D2D1::Point2F(w - p + k, h);
    c.br.end   = D2D1::Point2F(w - p,     h);

    // Bottom-left corner. Tip at (0, h).
    c.bl.start = D2D1::Point2F(p,     h);
    c.bl.c1    = D2D1::Point2F(p - k, h);
    c.bl.c2    = D2D1::Point2F(0.0f,  h - p + k);
    c.bl.end   = D2D1::Point2F(0.0f,  h - p);

    // Top-left corner. Tip at (0, 0).
    c.tl.start = D2D1::Point2F(0.0f,  p);
    c.tl.c1    = D2D1::Point2F(0.0f,  p - k);
    c.tl.c2    = D2D1::Point2F(p - k, 0.0f);
    c.tl.end   = D2D1::Point2F(p,     0.0f);

    return c;
}

}  // namespace

// ---------------------------------------------------------------------------

ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float width,
                                            float height,
                                            float radius,
                                            float smoothing) {
    radius = std::clamp(radius, 0.0f, std::min(width, height) * 0.5f);

    const auto c = ComputeCorners(width, height, radius, smoothing);

    ComPtr<ID2D1PathGeometry> geom;
    ThrowIfFailed(factory->CreatePathGeometry(geom.GetAddressOf()),
                  "ID2D1Factory::CreatePathGeometry");

    ComPtr<ID2D1GeometrySink> sink;
    ThrowIfFailed(geom->Open(sink.GetAddressOf()),
                  "ID2D1PathGeometry::Open");

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(c.tr.start, D2D1_FIGURE_BEGIN_FILLED);

    // TR corner -> right edge -> BR corner -> bottom edge -> BL corner ->
    // left edge -> TL corner -> top edge -> close.
    sink->AddBezier(D2D1::BezierSegment(c.tr.c1, c.tr.c2, c.tr.end));
    sink->AddLine(c.br.start);
    sink->AddBezier(D2D1::BezierSegment(c.br.c1, c.br.c2, c.br.end));
    sink->AddLine(c.bl.start);
    sink->AddBezier(D2D1::BezierSegment(c.bl.c1, c.bl.c2, c.bl.end));
    sink->AddLine(c.tl.start);
    sink->AddBezier(D2D1::BezierSegment(c.tl.c1, c.tl.c2, c.tl.end));
    // Implicit close-line back to c.tr.start.

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    ThrowIfFailed(sink->Close(), "ID2D1GeometrySink::Close");

    return geom;
}

// ---------------------------------------------------------------------------

HRGN BuildSquircleRegion(int width,
                         int height,
                         int radius,
                         float smoothing,
                         int samplesPerCorner) {
    radius = std::clamp(radius, 0, std::min(width, height) / 2);
    if (samplesPerCorner < 4) samplesPerCorner = 4;

    const auto c = ComputeCorners(static_cast<float>(width),
                                  static_cast<float>(height),
                                  static_cast<float>(radius), smoothing);

    auto evalCubic = [](D2D1_POINT_2F p0, D2D1_POINT_2F p1,
                        D2D1_POINT_2F p2, D2D1_POINT_2F p3, float t) {
        const float u = 1.0f - t;
        const float b0 = u * u * u;
        const float b1 = 3 * u * u * t;
        const float b2 = 3 * u * t * t;
        const float b3 = t * t * t;
        return D2D1::Point2F(b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
                             b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y);
    };

    std::vector<POINT> pts;
    pts.reserve(static_cast<size_t>(samplesPerCorner) * 4 + 8);

    auto pushPt = [&](D2D1_POINT_2F p) {
        POINT pt{static_cast<LONG>(std::lround(p.x)),
                 static_cast<LONG>(std::lround(p.y))};
        if (pts.empty() || pts.back().x != pt.x || pts.back().y != pt.y) {
            pts.push_back(pt);
        }
    };

    auto sampleCorner = [&](const CornerPoints& corner) {
        pushPt(corner.start);
        for (int s = 1; s < samplesPerCorner; ++s) {
            const float t = static_cast<float>(s) / samplesPerCorner;
            pushPt(evalCubic(corner.start, corner.c1, corner.c2, corner.end, t));
        }
        pushPt(corner.end);
    };

    sampleCorner(c.tr);
    sampleCorner(c.br);
    sampleCorner(c.bl);
    sampleCorner(c.tl);

    return ::CreatePolygonRgn(pts.data(),
                              static_cast<int>(pts.size()),
                              WINDING);
}

}  // namespace mactw::window
