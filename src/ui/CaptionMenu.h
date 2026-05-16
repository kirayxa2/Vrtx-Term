// Liquid Glass dropdown menu that grows out of the caption button.
//
// Visual recipe (matches the right-side toolbar dropdowns shipped in the
// stock macOS Tahoe apps - Mail, Notes, Settings):
//
//   - Squircle base shape, corner radius matches the host window so the
//     menu reads as part of the same chrome family.
//   - Translucent dark fill (`captionMenuFill`) with NO top highlight
//     band - the WWDC25 reference shows a perfectly even surface; the
//     "glass" feeling comes from the fill being semi-translucent and
//     the soft drop shadow underneath, not from a baked-in light strip.
//   - Soft drop shadow under the panel (offset 8pt, blur 20pt, alpha 30%)
//     so it floats above the terminal contents.
//   - No visible outline. Apple does not draw a hairline around toolbar
//     popovers; the panel reads as a free-floating piece of glass.
//
// Animation:
//
//   The window drives a unit-interval `progress_` from 0 (fully closed) to
//   1 (fully open) over `kCaptionMenuAnimDurationMs`, easing it through
//   an Apple-style cubic-out curve. `Render()` interprets `progress_` as
//   a per-frame transform:
//
//     scaleY    = 0.85 + 0.15 * progress             (subtle vertical grow)
//     opacity   = progress
//     glyph fade = smoothstep(0.3, 1.0, progress)    (items appear last)
//
//   The geometry origin is anchored to the centre top of the panel, so
//   the growth visually starts from the caption button's bottom edge.
//
// Items:
//
//   The menu owns a vector of `Item`s, each with a leading glyph (drawn
//   in Segoe UI Symbol so we never need a Nerd Font for chrome itself),
//   a label, and a click handler.
//
// Performance notes:
//
//   The DWrite text formats (label + icon) are cached on the instance
//   and only rebuilt when DPI/text size changes. Rebuilding them every
//   frame - which the previous revision did - made the open/close
//   animation visibly stutter. Same goes for solid-color brushes; the
//   menu now reuses the caller-supplied `brush` (its colour is changed
//   per-pass via SetColor) instead of allocating anything per-frame.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

class CaptionMenu {
public:
    using ItemHandler = std::function<void()>;

    struct Item {
        std::wstring glyph;       // single grapheme, e.g. L"\u2699" (gear)
        std::wstring label;       // localised display label
        ItemHandler  on_pick;     // invoked synchronously on click
    };

    void AddItem(Item item) { items_.push_back(std::move(item)); }

    // Lay out the panel directly under `anchor` (the caption button's
    // bounds), aligned to the anchor's right edge so the menu hangs from
    // the same vertical line as the chevron. `squircleWidthPx` is unused
    // today but kept for symmetry with other widgets in case we need to
    // clamp the panel to the squircle later.
    void UpdateLayout(D2D1_RECT_F anchor, int squircleWidthPx, UINT dpi);

    // Outer bounding rect of the panel (squircle-local, physical px).
    D2D1_RECT_F Bounds() const { return bounds_; }

    bool IsOpen() const { return open_; }

    // Set by the window. The menu becomes interactive (hit-testing,
    // hover) only when open AND fully expanded; while animating in/out
    // we still hit-test so users can race-click confidently, but we
    // suppress hover highlights during the first ~30% of the open anim
    // to prevent the eye from chasing a moving target.
    void SetOpen(bool open) { open_ = open; }
    void SetProgress(float p) { progress_ = std::clamp(p, 0.0f, 1.0f); }
    float Progress() const { return progress_; }

    // Hit-test the panel itself. Returns true when (x, y) is inside the
    // *animated* rect (we skip the hit when fully closed to avoid
    // intercepting clicks meant for the terminal underneath).
    bool HitTest(int x, int y) const;

    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    bool OnLButtonUp(int x, int y);   // returns true if an item fired

    void Render(ID2D1DeviceContext* dc,
                ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory,
                IDWriteFactory* dwrite) const;

private:
    // Returns the index of the item at (x, y), or -1.
    int ItemIndexAt(int x, int y) const;

    // Compute the y of an item row (top edge), in squircle-local px.
    float ItemTopY(int index) const;

    // Cached layout (squircle-local px). bounds_ is the *expanded* rect;
    // Render() shrinks it according to `progress_`.
    D2D1_RECT_F bounds_{};
    float       row_h_px_      {0};
    float       padding_x_px_  {0};   // panel outer padding
    float       padding_y_px_  {0};
    float       row_padding_x_px_{0}; // row inner padding
    float       gap_px_        {0};
    float       icon_size_px_  {0};
    float       icon_gap_px_   {0};
    float       text_size_px_  {0};
    float       radius_px_     {0};
    float       row_radius_px_ {0};

    bool   open_     {false};
    float  progress_ {0.0f};

    int    hover_index_  {-1};
    int    pressed_index_{-1};

    std::vector<Item> items_;

    // Cached DWrite formats. Rebuilt only when DPI/text size changes;
    // this matters because re-creating these on every frame is what
    // made the open/close animation stutter previously.
    mutable ComPtr<IDWriteTextFormat> label_fmt_;
    mutable ComPtr<IDWriteTextFormat> icon_fmt_;
    mutable float                     fmt_built_at_text_size_{0};
    mutable float                     fmt_built_at_icon_size_{0};
};

}  // namespace mactw::ui
