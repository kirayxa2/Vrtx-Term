// Live theme tweakers for visual A/B testing.
//
// Lets us bind keyboard shortcuts to values normally driven by `theme::`
// constants, and watch the window update without recompiling. Once the
// visuals are locked in we will hardcode the chosen numbers and remove
// this file.
//
// Bindings (window must be focused):
//
//   [    ]      corner radius      -1 / +1 pt    (Shift = +-4)
//   ,    .      tl disc diameter   -1 / +1 pt
//   ;    '      tl spacing         -1 / +1 pt    (between disc edges)
//   -    =      tl inset (left)    -1 / +1 pt    (window edge to first disc edge)
//   9    0      caption height     -1 / +1 pt

#pragma once

#include "pch.h"

namespace mactw::theme::tweaks {

extern float gCornerRadius;       // window corner radius
extern float gTlDiameter;         // traffic-light disc diameter
extern float gTlSpacing;          // edge-to-edge gap between adjacent discs
extern float gTlInsetX;           // window edge to left edge of first disc
extern float gCaptionHeight;      // caption strip height

// Returns true if the key consumed the event so the caller can request a
// repaint and re-layout the traffic lights.
bool HandleKey(WPARAM vk, bool shift);

}  // namespace mactw::theme::tweaks
