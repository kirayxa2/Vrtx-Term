// Liquid Glass dropdown menu that grows out of the caption button.
//
// Visual recipe (matches the right-side toolbar dropdowns shipped in the
// stock macOS Tahoe apps - Mail, Notes, Settings):
//
//   - Squircle base shape, ~10pt corner radius.
//   - Translucent dark fill (`captionMenuFill`) with a soft top-edge
//     highlight (`captionMenuTopHighlight`) to suggest a curved glassy
//     surface catching light from above.
//   - 1pt hairline outline using the same colour as the window border so
//     the menu reads as a piece of the same chrome family.
//   - Soft drop shadow under the panel (offset 4pt, blur 16pt, alpha 28%)
//     so it floats above the terminal contents.
//
// Animation:
//
//   The window drives a unit-interval `progress_` from 0 (fully closed) to
//   1 (fully open) over `kCaptionMenuAnimDurationMs`, easing it through
//   an Apple-style cubic-out curve. `Render()` interprets `progress_` as
//   a per-frame transform:
//
//     scaleY    = 0.6 + 0.4 * progress              (panel grows downward)
//     opacity   = progress
//     glyph fade = smoothstep(0.4, 1.0, progress)   (items appear last)
//
//   The geometry origin is anchored to the centre top of the panel, so
//   the growth visually starts from the caption button's bottom edge.
//
// Items:
//
//   The menu owns a vector of `Item`s, each with:
//     - a Unicode glyph drawn at the leading edge (we use stock symbols
//       that exist in Cascadia Code / Segoe UI Symbol so we never need a
//       Nerd Font for the chrome itself);
//     - a label string (UTF-16, already localised by the caller);
//     - a click handler.
//
//   The window forwards mouse events to the menu while it is open. On
//   click of an item, `on_pick_` fires and the window animates the menu
//   closed.

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
    float       row_h_px_  {0};
    float       padding_x_px_{0};
    float       padding_y_px_{0};
    float       gap_px_     {0};
    float       icon_size_px_{0};
    float       icon_gap_px_{0};
    float       text_size_px_{0};
    float       radius_px_  {0};
    float       border_px_  {0};

    bool   open_     {false};
    float  progress_ {0.0f};

    int    hover_index_  {-1};
    int    pressed_index_{-1};

    std::vector<Item> items_;

    // Dwrite text format is rebuilt lazily inside Render() from the
    // factory we get there, so the menu doesn't need an Initialize().
    mutable ComPtr<IDWriteTextFormat> fmt_;
    mutable float                     fmt_built_at_size_{0};
};

}  // namespace mactw::ui
