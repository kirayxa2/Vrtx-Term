#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

// ---------------------------------------------------------------------------
// Squircle-corner math.
//
// Each corner is one cubic Bezier. With four corners and four straight edges
// the closed shape is eight path commands total, which is exactly what we
// emit below.
//
// Derivation:
//
//   * Standard cubic-Bezier approximation of a 90 degree circular arc of
//     radius r uses control points offset by K*r where
//
//         K = (4/3) * tan(pi/8) = 0.55228...
//
//     This is the de-facto constant used throughout vector graphics (Postscript,
//     SVG renderers, browsers).
//
//   * For an Apple-style continuous corner we extend the corner's footprint
//     along each edge from r to p = (1 + smoothing) * r. The same K is used to
//     place the handles, but now scaled by p instead of r. At smoothing = 0
//     we recover the exact circular round-rect; as smoothing grows the corner
//     stretches outward along the edges, which is the visual signature of a
//     squircle.
//
// This is not G2-continuous in the strict mathematical sense — figma-squircle's
// three-Bezier-per-corner scheme is. However at the radii Apple uses (16, 20,
// 26 pt) the visual difference is sub-pixel on standard displays, and this
// formulation has the practical advantages of being short, fully analytic, and
// trivial to sample for the window region.
// ---------------------------------------------------------------------------

constexpr float kArcK = 0.55228474983079339f;  // 4/3 * tan(pi/8)

struct CornerXform {
    int   rotationSteps;     // CCW 90-degree steps applied before translation
    float tx, ty;
};

// Rotate point p by `steps` * 90deg CCW around origin.
inline D2D1_POINT_2F Rotate90Steps(D2D1_POINT_2F p, int steps) {
    float x = p.x, y = p.y;
    for (int i = 0; i < steps; ++i) {
        const float nx = -y;
        const float ny =  x;
        x = nx;
        y = ny;
    }
    return D2D1::Point2F(x, y);
}

inline D2D1_POINT_2F Apply(const CornerXform& xf, D2D1_POINT_2F p) {
    auto r = Rotate90Steps(p, xf.rotationSteps);
    return D2D1::Point2F(r.x + xf.tx, r.y + xf.ty);
}

// In the canonical (unrotated) frame the corner tip sits at (0, 0), the
// "incoming" edge runs along -X (from p_minus_x to 0), and the "outgoing"
// edge runs along +Y. We emit one cubic from start to end of the corner.
struct CornerCubic {
    D2D1_POINT_2F start, c1, c2, end;
};

CornerCubic ComputeCornerCubic(float radius, float smoothing) {
    smoothing = std::clamp(smoothing, 0.0f, 1.0f);
    const float p = (1.0f + smoothing) * radius;

    // Handle inset is K*p from the start/end along the perpendicular axis.
    const float h = kArcK * p;

    return CornerCubic{
        .start = D2D1::Point2F(-p,    0.0f),
        .c1    = D2D1::Point2F(-p + h, 0.0f),
        .c2    = D2D1::Point2F( 0.0f, p - h),
        .end   = D2D1::Point2F( 0.0f, p),
    };
}

// Per-corner transforms applied to the canonical frame so that:
//   index 0 -> top-right     corner at (W, 0)
//   index 1 -> bottom-right  corner at (W, H)
//   index 2 -> bottom-left   corner at (0, H)
//   index 3 -> top-left      corner at (0, 0)
//
// Rotation steps are chosen so that the corner's "incoming edge" (along -X
// in the local frame) maps to the actual incoming edge in the window frame,
// and "outgoing edge" (+Y in local) to the actual outgoing edge — preserving
// the clockwise traversal of the path as a whole.
std::array<CornerXform, 4> CornerTransforms(float w, float h) {
    return {{
        {0, w, 0.f},
        {3, w, h},
        {2, 0.f, h},
        {1, 0.f, 0.f},
    }};
}

}  // namespace

// ---------------------------------------------------------------------------

ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float width,
                                            float height,
                                            float radius,
                                            float smoothing) {
    radius = std::clamp(radius, 0.0f, std::min(width, height) * 0.5f);

    const auto cubic   = ComputeCornerCubic(radius, smoothing);
    const auto xforms  = CornerTransforms(width, height);

    ComPtr<ID2D1PathGeometry> geom;
    ThrowIfFailed(factory->CreatePathGeometry(geom.GetAddressOf()),
                  "ID2D1Factory::CreatePathGeometry");

    ComPtr<ID2D1GeometrySink> sink;
    ThrowIfFailed(geom->Open(sink.GetAddressOf()), "ID2D1PathGeometry::Open");

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(Apply(xforms[0], cubic.start), D2D1_FIGURE_BEGIN_FILLED);

    for (int i = 0; i < 4; ++i) {
        const auto& xf = xforms[i];
        sink->AddBezier(D2D1::BezierSegment(
            Apply(xf, cubic.c1), Apply(xf, cubic.c2), Apply(xf, cubic.end)));

        const int next = (i + 1) % 4;
        sink->AddLine(Apply(xforms[next], cubic.start));
    }

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
    radius    = std::clamp(radius, 0, std::min(width, height) / 2);
    smoothing = std::clamp(smoothing, 0.0f, 1.0f);

    const auto cubic  = ComputeCornerCubic(static_cast<float>(radius), smoothing);
    const auto xforms = CornerTransforms(static_cast<float>(width),
                                         static_cast<float>(height));

    auto evalBezier = [](D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_POINT_2F p2,
                         D2D1_POINT_2F p3, float t) {
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

    for (int i = 0; i < 4; ++i) {
        const auto& xf = xforms[i];

        const auto p0 = Apply(xf, cubic.start);
        const auto p1 = Apply(xf, cubic.c1);
        const auto p2 = Apply(xf, cubic.c2);
        const auto p3 = Apply(xf, cubic.end);

        pushPt(p0);
        for (int s = 1; s < samplesPerCorner; ++s) {
            const float t = static_cast<float>(s) / samplesPerCorner;
            pushPt(evalBezier(p0, p1, p2, p3, t));
        }
        pushPt(p3);
    }

    return ::CreatePolygonRgn(pts.data(),
                              static_cast<int>(pts.size()),
                              WINDING);
}

}  // namespace mactw::window
