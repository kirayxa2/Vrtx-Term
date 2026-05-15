// macOS-style "traffic light" buttons: close, minimize, maximize.
//
// The control owns its hit-testing and rendering. It exposes:
//   * UpdateLayoutDIPs(): receive its physical placement from the layout pass.
//   * HitTest(): used by WM_NCHITTEST so dragging the caption strip excludes
//     the buttons.
//   * OnMouseMove() / OnMouseLeave() / OnLButtonDown() / OnLButtonUp():
//     called from the window proc.
//   * Render(): paints the three discs and the hover glyphs.
//
// Behaviour mirrors macOS:
//   - All three are dimmed when the window is inactive.
//   - On hover anywhere over the group, the close/min/max glyphs appear inside
//     each disc.
//   - Click+release on a single button triggers the corresponding action.
//   - Click+drag outside a button cancels.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

enum class TrafficAction {
    None,
    Close,
    Minimize,
    Maximize,
};

class TrafficLights {
public:
    // Recompute disc positions in physical pixels for a given DPI. Origin is
    // the window's top-left.
    void UpdateLayout(UINT dpi);

    // Hit-test in physical-pixel window coordinates. Returns the action a
    // mouse-down at this point would target, or None if the point is outside
    // every disc.
    TrafficAction HitTest(int x, int y) const;

    // Mouse interaction. All coordinates are physical-pixel window-relative.
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    // Returns the action to fire (None if click was cancelled by dragging out).
    TrafficAction OnLButtonUp(int x, int y);

    // Paint the three discs. `windowActive` controls the muted color set.
    void Render(ID2D1DeviceContext* dc, ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory, bool windowActive) const;

    // Bounding rect of the entire group, used by the caption-strip hit-tester
    // to exclude these pixels from drag.
    D2D1_RECT_F GroupBounds() const { return group_bounds_; }

private:
    struct Disc {
        D2D1_POINT_2F center{};   // physical px
        float         radius{};
        TrafficAction action{TrafficAction::None};
        theme::Color  color{};
    };

    bool        ContainsAnyDisc(int x, int y) const;
    const Disc* DiscAt(int x, int y) const;

    void DrawGlyph(ID2D1DeviceContext* dc, ID2D1SolidColorBrush* brush,
                   ID2D1Factory* factory, const Disc& disc) const;

    std::array<Disc, 3> discs_{};
    D2D1_RECT_F         group_bounds_{};

    bool          group_hovered_{false};
    TrafficAction pressed_{TrafficAction::None};
};

}  // namespace mactw::ui
