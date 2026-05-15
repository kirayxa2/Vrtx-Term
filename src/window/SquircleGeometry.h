// Apple `.continuous` / Figma corner-smoothing squircle path.
//
// We hardcode smoothing = 0.6 (the value Apple uses for iOS-7+ icons and
// SwiftUI's `.continuous` style). The five Figma coefficients for that
// smoothing are listed in the .cpp.

#pragma once

#include "pch.h"

namespace mactw::window {

// Build a closed squircle-shaped path in the rectangle [0, w] x [0, h].
//
// `radius` is the corner radius in physical pixels.
// `smoothing` is currently ignored (constant 0.6 is hardcoded). Argument is
// kept for API stability so the call sites do not have to change when we
// add per-radius smoothing later.
ComPtr<ID2D1PathGeometry> BuildSquirclePath(ID2D1Factory* factory,
                                            float w,
                                            float h,
                                            float radius,
                                            float smoothing = 0.6f);

}  // namespace mactw::window
