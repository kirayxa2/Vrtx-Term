// Apple Tahoe-style "more" / "dropdown" button in the top-right of the
// caption strip.
//
// Visually this is a circular Liquid Glass disc, ~28pt across, that holds
// an SF-Symbols-style chevron-down glyph. The fill is always visible
// (faint dark wash + 1pt hairline outline matching the window border);
// hover and press add a translucent overlay on top.
//
// When the caption-menu is open the chevron flips 180deg (points up) and
// the button stays in its "expanded" visual state. The flip is animated
// outside this class, by feeding `expansion_` (0..1) from the window's
// animation clock into SetExpansion().
//
// Coordinates throughout are *physical pixels relative to the squircle
// origin* (not the HWND), matching the convention TrafficLights uses.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

class CaptionButton {
public:
    using ClickHandler = std::function<void()>;

    // Recompute layout given the current squircle width (in physical px)
    // and DPI. Call from the window after every resize / DPI change.
    void UpdateLayout(int squircleWidthPx, UINT dpi);

    // True when (x, y) is inside the button's disc bounds.
    bool HitTest(int x, int y) const;

    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);

    // Returns true if the click landed and on_click_ should fire (which
    // we already invoked synchronously inside this call). The window
    // doesn't have to do anything special with the return value beyond
    // suppressing fall-through to other UI elements.
    bool OnLButtonUp(int x, int y);

    void Render(ID2D1DeviceContext* dc, ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory, bool windowActive) const;

    // Hand-off point for the user-supplied action. The handler runs on the
    // UI thread synchronously from inside OnLButtonUp().
    void SetOnClick(ClickHandler h) { on_click_ = std::move(h); }

    // Animation hook. `expansion` is a unit-interval interpolation
    // factor where 0 = fully closed (chevron points down) and 1 = fully
    // open (chevron points up). The window's animation tick drives this
    // value over `kCaptionMenuAnimDurationMs` while the menu is opening
    // or closing, so the rotation tracks the menu's appearance smoothly.
    void SetExpansion(float expansion) { expansion_ = expansion; }

    // Geometry accessor used by the popup menu so it can anchor its top
    // edge to the bottom of the button (in squircle-local pixels).
    D2D1_RECT_F Bounds() const { return bounds_; }

private:
    D2D1_RECT_F bounds_{};   // disc rect in squircle-local px

    bool hovered_{false};
    bool pressed_{false};

    // Animated rotation factor, driven externally by the window's tick.
    // Capacity beyond [0, 1] is harmless (the rotation just overshoots),
    // but the window currently clamps the timeline so values stay inside.
    float expansion_{0.0f};

    ClickHandler on_click_;
};

}  // namespace mactw::ui
