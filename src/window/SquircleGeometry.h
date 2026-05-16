// Continuous-corner (squircle) path geometry.
//
// We hardcode smoothing = 0.6 (the standard continuous-corner smoothing
// our chrome uses). The five smoothing coefficients for that value are
// listed in the .cpp.

#pragma once

#include "pch.h"

namespace vrtx::window {

// Build a closed squircle-shaped path in the rectangle [0, w] x [0, h].
//
// `radius` is the corner radius in physical pixels.
// `smoothing` is currently ignored (constant 0.6 is hardcoded). Kept for
// API stability so call sites do not need to change when we add
// per-corner smoothing later.
ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float w,
                                            float h,
                                            float radius,
                                            float smoothing = 0.6f);

// Same shape but positioned absolutely: builds a squircle inscribed in
// `rect` (squircle-local px). Useful for floating chrome elements -
// sidebar pills, content cards, toggle pills - so the call site does
// not have to push a translation transform.
ComPtr<ID2D1PathGeometry> BuildSquirclePathInRect(ID2D1Factory* factory,
                                                  D2D1_RECT_F rect,
                                                  float radius,
                                                  float smoothing = 0.6f);

}  // namespace vrtx::window
