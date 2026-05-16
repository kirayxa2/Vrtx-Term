// In-window Settings sheet rendered through D2D - macOS 26 Tahoe System
// Settings clone.
//
// Anatomy:
//
//   +---------------------------------------------------------------+
//   | (caption strip stays visible: traffic lights + drag still OK) |
//   +---------------------------------------------------------------+
//   |  +-------------------------------------------------+          |
//   |  |  +----------+   ┊                       [Done]  |          |
//   |  |  | sidebar  |   ┊   <pane title>                |          |
//   |  |  |          |   ┊                               |          |
//   |  |  | [G] Gen  |   ┊   +-------------------------+ |          |
//   |  |  | [A] App  |   ┊   |    body card            | |          |
//   |  |  | [P] Prom |   ┊   +-------------------------+ |          |
//   |  |  |  ...     |   ┊                               |          |
//   |  +-------------------------------------------------+          |
//   +---------------------------------------------------------------+
//
//   * One outer rounded translucent shell. Apple does not draw two
//     separate cards - sidebar and content are cells in the same
//     panel, separated by a 1pt hairline.
//   * Each sidebar row has a saturated coloured "tile" with a white
//     glyph on it (the SF Symbols-tinted look). This is the most
//     recognisable cue of Tahoe System Settings; without it, the
//     sheet looks generic.
//   * Active row is filled with the system blue pill, label turns
//     white. Hover is a faint translucent overlay.
//   * Top-right "Done" pill closes the sheet (Tahoe replaces the
//     Sequoia X/circle with this button).
//
// All sizes (sidebar width, paddings, radii, etc.) live in
// theme::TahoeTheme.h. The class is intentionally just a view: it
// doesn't own settings state. Each row's `on_pick` is wired by
// Application::Run so handlers can mutate the live profile / theme
// later.

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
        std::wstring   body;        // wrapped paragraph shown in the content pane
        theme::Color   tile_color;  // tile background (saturated SF colour)
        ItemHandler    on_pick;     // optional, fires when a row is clicked
    };

    void AddItem(Item it) { items_.push_back(std::move(it)); }

    // Lay out the sheet inside the provided squircle rect (squircle-local
    // pixels; same coordinate space the renderer uses for everything else
    // under the squircle clip).
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

    // Hit-test the entire sheet (sidebar + content). The window uses this
    // to decide whether to swallow a mouse event before the terminal
    // selection logic sees it.
    bool HitTestPanel(int x, int y) const;

    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    // Returns true if the user closed the sheet via the Done button.
    bool OnLButtonUp(int x, int y);

    void Render(ID2D1DeviceContext* dc,
                ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory,
                IDWriteFactory* dwrite) const;

    // Active row index (which section is shown in the content pane).
    void SetActiveIndex(int i) {
        if (i >= 0 && i < static_cast<int>(items_.size())) active_index_ = i;
    }
    int ActiveIndex() const { return active_index_; }

    // Optional callback fired when the user taps Done. The window
    // typically responds by starting the close animation.
    void SetOnDone(DoneHandler h) { on_done_ = std::move(h); }

    // Localised label for the primary Done button (e.g. "Done" /
    // "Готово"). Defaults to "Done" so callers that don't bother still
    // get a sane fallback.
    void SetDoneLabel(std::wstring s) { done_label_ = std::move(s); }

private:
    int  RowAt(int x, int y) const;
    bool DoneAt(int x, int y) const;

    // Layout cache (squircle-local px).
    D2D1_RECT_F shell_         {};   // outer rounded panel
    D2D1_RECT_F sidebar_       {};   // sidebar cell (tinted band)
    D2D1_RECT_F content_       {};   // content cell
    D2D1_RECT_F done_btn_      {};   // top-right Done pill
    float       outer_pad_px_  {0};
    float       shell_radius_px_{0};
    float       row_h_px_      {0};
    float       row_gap_px_    {0};
    float       row_pad_x_px_  {0};
    float       row_radius_px_ {0};
    float       row_side_pad_px_{0};
    float       tile_size_px_  {0};
    float       tile_radius_px_{0};
    float       tile_glyph_px_ {0};
    float       tile_text_gap_px_{0};
    float       row_text_px_   {0};
    float       sidebar_top_gap_px_{0};
    float       sidebar_bot_gap_px_{0};
    float       content_pad_x_px_{0};
    float       content_pad_y_px_{0};
    float       title_text_px_ {0};
    float       title_bot_gap_px_{0};
    float       body_text_px_  {0};
    float       card_radius_px_{0};
    float       card_pad_x_px_ {0};
    float       card_pad_y_px_ {0};
    float       done_text_px_  {0};
    float       sep_w_px_      {0};

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
