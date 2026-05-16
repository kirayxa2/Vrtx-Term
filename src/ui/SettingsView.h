// In-window Settings sheet rendered through D2D - macOS 26 Tahoe
// System Settings clone.
//
// Layout (no scrim, no blur — the sheet **replaces** the terminal area):
//
//   +--squircle (caption strip stays at top)---------------------------+
//   | TL  TL  TL                                              [Done]   |
//   |                                                                  |
//   |  +--sidebar pill---------+                                       |
//   |  |  General              |    Title 28pt                         |
//   |  |  Appearance           |                                       |
//   |  |  Prompt               |    Controls land directly on the      |
//   |  |  Terminal             |    squircle (no card wrapper).        |
//   |  |  About                |                                       |
//   |  +-----------------------+                                       |
//   +------------------------------------------------------------------+
//
//   * Sidebar is a separate floating rounded pill. Its corner radius
//     equals the window radius (16pt). Traffic-lights overlap its top-
//     left corner, exactly like Tahoe System Settings.
//   * Content is the bare squircle to the right of the sidebar with a
//     small gutter between them. No card / wrapper is drawn under the
//     controls; future toggles / pickers / sliders will sit directly
//     on the window surface.
//   * Done is a system-blue pill in the top-right of the caption
//     strip (it replaces the chrome chevron-button while open).
//
// The view is a pure renderer: it owns no settings state. Each row's
// `on_pick` is wired by the host so handlers can mutate the live
// profile / theme.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

class SettingsView {
public:
    using ItemHandler = std::function<void()>;
    using DoneHandler = std::function<void()>;

    struct Item {
        std::wstring   glyph;       // single grapheme drawn on the tile
        std::wstring   label;       // localised
        std::wstring   title;       // shown as the main pane title when active
        std::wstring   body;        // wrapped paragraph in content pane
        theme::Color   tile_color;  // tile background (saturated SF colour)
        ItemHandler    on_pick;     // optional, fires when row is clicked
    };

    void AddItem(Item it) { items_.push_back(std::move(it)); }

    // Lay out the sheet inside the squircle (squircle-local px).
    // captionHeightPx is the bottom of the caption strip.
    void UpdateLayout(D2D1_RECT_F squircleRect, float captionHeightPx,
                      UINT dpi);

    bool IsOpen()      const { return open_; }
    bool IsClosing()   const { return closing_; }

    void Show()           { open_ = true;  closing_ = false;
                             hover_index_ = -1; pressed_index_ = -1;
                             hover_done_ = false; pressed_done_ = false; }
    void RequestClose()   { closing_ = true; }
    void SetOpen(bool v) {
        open_ = v;
        if (!v) {
            closing_ = false;
            hover_index_ = -1;
            pressed_index_ = -1;
            hover_done_ = false;
            pressed_done_ = false;
        }
    }
    void SetProgress(float p) { progress_ = std::clamp(p, 0.0f, 1.0f); }
    float Progress() const    { return progress_; }

    // Done button bounds in squircle-local px - the window asks for
    // them so it can hit-test the button against caption-strip clicks
    // (its rect overlaps the chrome chevron-button area).
    D2D1_RECT_F DoneBounds() const { return done_btn_; }
    bool        HitTestDone(int x, int y) const { return DoneAt(x, y); }

    // Hit-test the sidebar pill (so the window can swallow clicks there
    // before the terminal sees them).
    bool HitTestSidebar(int x, int y) const;

    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    // Returns true if the user activated Done (host should close).
    bool OnLButtonUp(int x, int y);

    void Render(ID2D1DeviceContext* dc,
                ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory,
                IDWriteFactory* dwrite) const;

    // Active row: which section is shown in the content pane.
    void SetActiveIndex(int i) {
        if (i >= 0 && i < static_cast<int>(items_.size())) active_index_ = i;
    }
    int ActiveIndex() const { return active_index_; }

    // Optional callback fired when the user taps Done.
    void SetOnDone(DoneHandler h) { on_done_ = std::move(h); }

    // Localised label of the Done button ("Done" / "Готово").
    void SetDoneLabel(std::wstring s) { done_label_ = std::move(s); }

private:
    int  RowAt(int x, int y) const;
    bool DoneAt(int x, int y) const;

    // Layout cache (squircle-local px).
    D2D1_RECT_F sidebar_       {};   // floating sidebar pill
    D2D1_RECT_F content_       {};   // bare squircle area for the pane
    D2D1_RECT_F done_btn_      {};   // top-right Done pill

    float       sidebar_radius_px_  {0};
    float       row_h_px_           {0};
    float       row_gap_px_         {0};
    float       row_pad_x_px_       {0};
    float       row_radius_px_      {0};
    float       row_side_pad_px_    {0};
    float       tile_size_px_       {0};
    float       tile_radius_px_     {0};
    float       tile_glyph_px_      {0};
    float       tile_text_gap_px_   {0};
    float       row_text_px_        {0};
    float       sidebar_top_gap_px_ {0};
    float       sidebar_bot_gap_px_ {0};
    float       content_pad_x_px_   {0};
    float       content_pad_top_px_ {0};
    float       title_text_px_      {0};
    float       title_bot_gap_px_   {0};
    float       body_text_px_       {0};
    float       done_text_px_       {0};

    bool   open_     {false};
    bool   closing_  {false};
    float  progress_ {0.0f};

    int    active_index_  {0};
    int    hover_index_   {-1};
    int    pressed_index_ {-1};
    bool   hover_done_    {false};
    bool   pressed_done_  {false};

    std::vector<Item> items_;
    DoneHandler       on_done_;
    std::wstring      done_label_{L"Done"};

    // DWrite format cache: built once per DPI, reused across frames.
    mutable ComPtr<IDWriteTextFormat> row_fmt_;
    mutable ComPtr<IDWriteTextFormat> tile_fmt_;
    mutable ComPtr<IDWriteTextFormat> title_fmt_;
    mutable ComPtr<IDWriteTextFormat> body_fmt_;
    mutable ComPtr<IDWriteTextFormat> done_fmt_;
    mutable float built_at_row_   {0};
    mutable float built_at_tile_  {0};
    mutable float built_at_title_ {0};
    mutable float built_at_body_  {0};
    mutable float built_at_done_  {0};
};

}  // namespace mactw::ui
