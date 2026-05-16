// Round "traffic light" buttons: close, minimize, maximize.
//
// The control owns its hit-testing and rendering. It exposes:
//   * UpdateLayoutDIPs(): receive its physical placement from the layout pass.
//   * HitTest(): used by WM_NCHITTEST so dragging the caption strip excludes
//     the buttons.
//   * OnMouseMove() / OnMouseLeave() / OnLButtonDown() / OnLButtonUp():
//     called from the window proc.
//   * Render(): paints the three discs and the hover glyphs.
//
// Behaviour:
//   - All three are dimmed when the window is inactive.
//   - On hover anywhere over the group, the close/min/max glyphs appear inside
//     each disc.
//   - Click+release on a single button triggers the corresponding action.
//   - Click+drag outside a button cancels.

#pragma once

#include "pch.h"
#include "theme/AppTheme.h"

namespace vrtx::ui {

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

    // Apply an additional offset to the disc row, in physical pixels.
    // Used when the Settings sheet is open: the sidebar pill begins
    // close to the squircle's left edge with a 16pt rounded corner,
    // so the lights need a couple of px to clear the corner and not
    // look glued to it. Pass 0 to restore the default placement.
    void SetXShift(float pxOffset);
    void SetYShift(float pxOffset);

    // Bounding rect of the entire group, used by the caption-strip hit-tester
    // to exclude these pixels from drag.
    D2D1_RECT_F GroupBounds() const { return group_bounds_; }
    bool IsGroupHovered()     const { return group_hovered_; }

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
                   ID2D1Factory* factory, const Disc& disc, float alpha) const;

    std::array<Disc, 3> discs_{};
    D2D1_RECT_F         group_bounds_{};

    // Latest DPI from UpdateLayout(); cached so SetXShift() can re-run
    // the layout against the same DPI without the host having to pass
    // it again.
    UINT  layout_dpi_{96};

    // Extra offsets added to the disc row's base position (physical
    // pixels). Persist across UpdateLayout() calls so the host doesn't
    // have to re-apply them after every resize / DPI change. Reset
    // explicitly via SetXShift(0) / SetYShift(0).
    float x_shift_px_{0.0f};
    float y_shift_px_{0.0f};

    bool          group_hovered_{false};
    TrafficAction pressed_{TrafficAction::None};

    // Glyph fade: 0.0 = invisible, 1.0 = fully visible.
    // Driven by the window's traffic-light timer (60 Hz).
    // The Render() method reads this value directly instead of
    // doing a hard on/off based on group_hovered_.
    mutable float glyph_alpha_{0.0f};

public:
    // Called by BorderlessWindow::OnTrafficTimer() every ~16ms.
    // Returns true while the animation is still running.
    bool TickGlyphFade(bool targetVisible, float dt);
    float GlyphAlpha() const { return glyph_alpha_; }
private:
};

}  // namespace vrtx::ui
