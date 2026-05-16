// Apple Tahoe-style "more" / "dropdown" button in the top-right of the
// caption strip.
//
// Visually this is a pill-shaped slot, ~32x22 pt, that holds a small
// chevron-down glyph. Default state has no fill - only the glyph is
// visible against the window tint. On hover the pill gets a subtle
// translucent background; on press the background becomes a touch more
// solid. Matches the look of the sidebar/split-view buttons that ship in
// macOS 26 Tahoe windows and the inspector toggles in Mail/Notes.
//
// Behaviour:
//   - Hover state tracked at the *button* level, not group-wise (we have
//     only one button right now).
//   - Press-then-release inside the button fires `on_click_`. Press
//     followed by drag-out cancels the click.
//   - When the window is inactive the button keeps its layout but skips
//     the hover/press visuals so the chrome looks dim, like the rest of
//     the inactive window.
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

    // True when (x, y) is inside the button's pill bounds.
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

private:
    D2D1_RECT_F bounds_{};   // pill rect in squircle-local px

    bool hovered_{false};
    bool pressed_{false};

    ClickHandler on_click_;
};

}  // namespace mactw::ui
