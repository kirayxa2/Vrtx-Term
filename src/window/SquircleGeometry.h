// Squircle (G2-continuous, Apple `.continuous` style) corner geometry.
//
// Apple's rounded rectangles are not arc-based. Each corner blends out of the
// straight edge with curvature continuity: the curvature starts at 0 on the
// edge, smoothly rises to its peak at 45 degrees into the corner, and back
// down. This is what figma-squircle and SwiftUI
// `RoundedRectangle(cornerRadius: r, style: .continuous)` produce.
//
// We approximate it with three cubic Bezier segments per corner. Two outer
// "shoulder" beziers handle the smooth curvature ramp; an inner one
// approximates the central arc. The math here is the same scheme used by
// figma-squircle and matches Apple to within a sub-pixel for radii up to 50pt.

#pragma once

#include "pch.h"

namespace mactw::window {

// Build a squircle path filling [0,0]-[width,height] in physical pixels.
// `radius` is the corner radius in physical pixels. `smoothing` is the Apple
// continuity factor in [0, 1]; 0 produces a regular circular round-rect, ~0.6
// matches Apple's `.continuous` look.
//
// The returned geometry is a closed filled shape suitable for FillGeometry /
// DrawGeometry (we don't bake the stroke into the path).
ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float width,
                                            float height,
                                            float radius,
                                            float smoothing);

// Build an HRGN approximating the same squircle, used for SetWindowRgn so the
// system-level acrylic backdrop is clipped to the squircle. We tessellate the
// path to a polygon with `samplesPerCorner` points; 24 looks indistinguishable
// from the geometry path at typical window sizes.
HRGN BuildSquircleRegion(int width,
                         int height,
                         int radius,
                         float smoothing,
                         int samplesPerCorner = 24);

}  // namespace mactw::window
