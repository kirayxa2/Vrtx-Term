// Live theme tweakers for visual A/B testing.
//
// While dialling in macOS-Tahoe-accurate corners, recompiling between every
// adjustment is slow. RuntimeTweaks lets us bind keyboard shortcuts to
// values normally driven by `theme::` constants and watch the window
// update without leaving the running app. Once the visuals are locked in
// we will hardcode the chosen numbers and remove this file.

#pragma once

#include "pch.h"

namespace mactw::theme::tweaks {

// All values in logical pt.
extern float gCornerRadius;        // 16 pt default (Apple titlebar window)

// `[` / `]` decrease / increase the radius by 1 pt; `Shift` makes the
// step 4 pt. Returns true if the key consumed the event so the caller
// can request a repaint.
bool HandleKey(WPARAM vk, bool shift);

}  // namespace mactw::theme::tweaks
