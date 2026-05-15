#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

// ---------------------------------------------------------------------------
// Apple `.continuous` / figma-squircle corner geometry.
//
// Each corner is rendered as a chain of THREE cubic Beziers, not one. This
// is what gives the corner G2 (curvature-continuous) flow: the curvature
// rises smoothly from 0 on the straight edge, peaks at 45 degrees into the
// corner, and decays back to 0 - rather than the sudden curvature jump you
// get from a single circular-arc-approximation Bezier.
//
// The math below is a direct port of the figma-squircle reference
// implementation (MartinRGB / phamfoo / figma-squircle on npm), which itself
// was reverse-engineered from Apple's published `.continuous` shape.
//
// Per corner, the chain is:
//
//     edge end -> [shoulder out] -> [arc] -> [shoulder in] -> next edge end
//
// where each bracketed segment is one cubic Bezier. The two shoulders
// handle the smooth ramp into and out of curvature; the central arc is a
// near-circular bend.
// ---------------------------------------------------------------------------

struct ContinuousCorner {
    D2D1_POINT_2F start;          // entry point on the straight edge
    D2D1_POINT_2F c1a, c1b, m1;   // shoulder out: start -> m1
    D2D1_POINT_2F c2a, c2b, m2;   // arc:          m1 -> m2
    D2D1_POINT_2F c3a, c3b, end;  // shoulder in:  m2 -> end (next straight edge)
};

inline float DegToRad(float d) { return d * 3.14159265358979323846f / 180.0f; }

// Compute a single corner in the "canonical frame" where:
//   - the corner tip is at (0, 0)
//   - the incoming straight edge runs along -X (entering from x = -inf)
//   - the outgoing straight edge runs along +Y (leaving toward y = +inf)
// All four window corners use this same canonical chain, just rotated.
ContinuousCorner ComputeCanonicalCorner(float radius, float smoothing) {
    smoothing = std::clamp(smoothing, 0.0f, 1.0f);

    // figma-squircle's intermediate variables.
    //
    //   p     = how far the corner footprint extends along each edge,
    //           starting at the corner tip. At smoothing == 0 this collapses
    //           to r and we recover a plain circular arc; as smoothing grows
    //           toward 1, p grows toward 2*r and the corner spreads out.
    //
    //   theta = arc angle of the central circular segment, in degrees.
    //           45deg for a pure circle, decreases as smoothing increases
    //           (the shoulders take over more of the angular sweep).

    const float p     = (1.0f + smoothing) * radius;
    const float theta = (1.0f - smoothing) * 90.0f * 0.5f;        // degrees
    const float arcAngle = DegToRad(theta);

    // Length of the chord the central arc spans, projected onto the edge.
    const float arcSectionLength = std::sin(arcAngle) * radius * std::sqrt(2.0f);

    // Bezier handle length to approximate the central arc as a cubic, scaled
    // for the smaller arc angle.
    const float arcK = (4.0f / 3.0f) * std::tan(arcAngle / 2.0f);
    const float arcHandle = arcK * radius;

    // How the remaining `p - arcSectionLength` budget is split between the
    // two shoulder control points along the edge.
    const float a = (p - arcSectionLength) / 3.0f;
    const float b = 2.0f * a;

    // Inscribed-circle centre for this corner sits at (-r, +r) in the
    // canonical frame.
    const float cx = -radius;
    const float cy =  radius;

    const float arcStartAngle = DegToRad(180.0f + theta);
    const float arcEndAngle   = DegToRad(270.0f - theta);

    const D2D1_POINT_2F arcStart = D2D1::Point2F(
        cx + radius * std::cos(arcStartAngle),
        cy + radius * std::sin(arcStartAngle));
    const D2D1_POINT_2F arcEnd = D2D1::Point2F(
        cx + radius * std::cos(arcEndAngle),
        cy + radius * std::sin(arcEndAngle));

    // Tangent vectors at arcStart / arcEnd, pointing in the direction of
    // path traversal (CCW around the arc centre).
    const D2D1_POINT_2F arcStartTangent = D2D1::Point2F(
        -std::sin(arcStartAngle), std::cos(arcStartAngle));
    const D2D1_POINT_2F arcEndTangent = D2D1::Point2F(
        -std::sin(arcEndAngle), std::cos(arcEndAngle));

    ContinuousCorner cc{};
    cc.start = D2D1::Point2F(-p, 0.0f);

    // ---- Shoulder out: from (-p, 0) to arcStart ---------------------------
    cc.c1a = D2D1::Point2F(-p + b, 0.0f);
    cc.c1b = D2D1::Point2F(arcStart.x - arcStartTangent.x * a,
                           arcStart.y - arcStartTangent.y * a);
    cc.m1  = arcStart;

    // ---- Central arc as one cubic: arcStart -> arcEnd ---------------------
    cc.c2a = D2D1::Point2F(arcStart.x + arcStartTangent.x * arcHandle,
                           arcStart.y + arcStartTangent.y * arcHandle);
    cc.c2b = D2D1::Point2F(arcEnd.x   - arcEndTangent.x   * arcHandle,
                           arcEnd.y   - arcEndTangent.y   * arcHandle);
    cc.m2  = arcEnd;

    // ---- Shoulder in: arcEnd -> (0, p) ------------------------------------
    cc.c3a = D2D1::Point2F(arcEnd.x + arcEndTangent.x * a,
                           arcEnd.y + arcEndTangent.y * a);
    cc.c3b = D2D1::Point2F(0.0f, p - b);
    cc.end = D2D1::Point2F(0.0f, p);

    return cc;
}

// Per-corner placement: 90-degree CCW rotation steps applied to the
// canonical frame, plus a translation onto the corner tip in window space.
struct CornerPlacement {
    int   rotationSteps;
    float tx, ty;
};

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

inline D2D1_POINT_2F Apply(const CornerPlacement& xf, D2D1_POINT_2F p) {
    auto r = Rotate90Steps(p, xf.rotationSteps);
    return D2D1::Point2F(r.x + xf.tx, r.y + xf.ty);
}

// Place the canonical corner (tip at origin, incoming -X, outgoing +Y) at
// each of the four window corners while keeping clockwise traversal.
//   index 0 -> top-right
//   index 1 -> bottom-right
//   index 2 -> bottom-left
//   index 3 -> top-left
std::array<CornerPlacement, 4> WindowCornerPlacements(float w, float h) {
    return {{
        {0, w,    0.0f},
        {3, w,    h   },
        {2, 0.0f, h   },
        {1, 0.0f, 0.0f},
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

    const auto canonical = ComputeCanonicalCorner(radius, smoothing);
    const auto places    = WindowCornerPlacements(width, height);

    ComPtr<ID2D1PathGeometry> geom;
    ThrowIfFailed(factory->CreatePathGeometry(geom.GetAddressOf()),
                  "ID2D1Factory::CreatePathGeometry");

    ComPtr<ID2D1GeometrySink> sink;
    ThrowIfFailed(geom->Open(sink.GetAddressOf()),
                  "ID2D1PathGeometry::Open");

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(Apply(places[0], canonical.start),
                      D2D1_FIGURE_BEGIN_FILLED);

    for (int i = 0; i < 4; ++i) {
        const auto& xf = places[i];

        sink->AddBezier(D2D1::BezierSegment(
            Apply(xf, canonical.c1a),
            Apply(xf, canonical.c1b),
            Apply(xf, canonical.m1)));
        sink->AddBezier(D2D1::BezierSegment(
            Apply(xf, canonical.c2a),
            Apply(xf, canonical.c2b),
            Apply(xf, canonical.m2)));
        sink->AddBezier(D2D1::BezierSegment(
            Apply(xf, canonical.c3a),
            Apply(xf, canonical.c3b),
            Apply(xf, canonical.end)));

        const int next = (i + 1) % 4;
        sink->AddLine(Apply(places[next], canonical.start));
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
    radius = std::clamp(radius, 0, std::min(width, height) / 2);
    if (samplesPerCorner < 4) samplesPerCorner = 4;

    const auto canonical = ComputeCanonicalCorner(static_cast<float>(radius),
                                                  smoothing);
    const auto places    = WindowCornerPlacements(static_cast<float>(width),
                                                  static_cast<float>(height));

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
    pts.reserve(static_cast<size_t>(samplesPerCorner) * 12 + 8);

    auto pushPt = [&](D2D1_POINT_2F p) {
        POINT pt{static_cast<LONG>(std::lround(p.x)),
                 static_cast<LONG>(std::lround(p.y))};
        if (pts.empty() || pts.back().x != pt.x || pts.back().y != pt.y) {
            pts.push_back(pt);
        }
    };

    auto sampleCubic = [&](D2D1_POINT_2F p0, D2D1_POINT_2F p1,
                           D2D1_POINT_2F p2, D2D1_POINT_2F p3) {
        pushPt(p0);
        for (int s = 1; s < samplesPerCorner; ++s) {
            const float t = static_cast<float>(s) / samplesPerCorner;
            pushPt(evalCubic(p0, p1, p2, p3, t));
        }
        pushPt(p3);
    };

    for (int i = 0; i < 4; ++i) {
        const auto& xf = places[i];
        sampleCubic(Apply(xf, canonical.start),
                    Apply(xf, canonical.c1a),
                    Apply(xf, canonical.c1b),
                    Apply(xf, canonical.m1));
        sampleCubic(Apply(xf, canonical.m1),
                    Apply(xf, canonical.c2a),
                    Apply(xf, canonical.c2b),
                    Apply(xf, canonical.m2));
        sampleCubic(Apply(xf, canonical.m2),
                    Apply(xf, canonical.c3a),
                    Apply(xf, canonical.c3b),
                    Apply(xf, canonical.end));
    }

    return ::CreatePolygonRgn(pts.data(),
                              static_cast<int>(pts.size()),
                              WINDING);
}

}  // namespace mactw::window
