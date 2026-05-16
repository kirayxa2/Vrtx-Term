// In-window Settings sheet rendered through D2D - macOS Tahoe System
// Settings clone (sidebar on the left, content pane on the right).
//
// Why a separate component instead of opening another HWND:
//
//   - The whole point of MacTermWin is that everything inside the window
//     belongs to us. A foreign Win32 dialog would bring back the DWM
//     decorations, square corners, and the wrong fonts.
//   - Animations stay in sync with the rest of the chrome. The Settings
//     sheet can fade in over the terminal grid without crossing process
//     or composition boundaries.
//
// Layout, top to bottom of the squircle (caption strip excluded):
//
//   +------------------------------------------------------------+
//   | (caption strip stays visible: traffic lights still work)   |
//   +-----------------+------------------------------------------+
//   |                 |                                          |
//   |   sidebar       |   content pane                           |
//   |   - General     |                                          |
//   |   - Appearance  |   <pane title>                           |
//   |   - Prompt      |                                          |
//   |   - Terminal    |   <body for the active section>          |
//   |   - About       |                                          |
//   |                 |                                          |
//   |                 |                                          |
//   |                 |                                          |
//   +-----------------+------------------------------------------+
//
// All sizes (sidebar width, paddings, radii, etc.) live in
// theme::TahoeTheme.h. The class is intentionally just a view: it
// doesn't own settings state. Each row's `on_pick` is wired by
// Application::Run so handlers can mutate the live profile / theme
// later.
//
// API mirrors CaptionMenu / AppAlert so the host window can drive the
// open/close animation with a simple `progress_` float.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

class SettingsView {
public:
    using ItemHandler = std::function<void()>;

    struct Item {
        std::wstring glyph;     // single grapheme (Segoe UI Symbol)
        std::wstring label;     // localised
        std::wstring title;     // shown as the main pane title when active
        std::wstring body;      // wrapped paragraph shown in the content pane
        ItemHandler  on_pick;   // optional, fires when a row is clicked
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
                             hover_index_ = -1; pressed_index_ = -1; }
    void RequestClose()   { closing_ = true; }
    void SetOpen(bool v) {
        open_ = v;
        if (!v) {
            closing_ = false;
            hover_index_ = -1;
            pressed_index_ = -1;
            hover_close_ = false;
            pressed_close_ = false;
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
    // Returns true if the user closed the sheet via the close-button.
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

private:
    int RowAt(int x, int y) const;
    bool CloseAt(int x, int y) const;

    // Layout cache (squircle-local px).
    D2D1_RECT_F sidebar_       {};
    D2D1_RECT_F content_       {};
    D2D1_RECT_F close_btn_     {};
    float       outer_pad_px_  {0};
    float       pane_radius_px_{0};
    float       row_h_px_      {0};
    float       row_gap_px_    {0};
    float       row_pad_x_px_  {0};
    float       row_radius_px_ {0};
    float       icon_size_px_  {0};
    float       icon_gap_px_   {0};
    float       row_text_px_   {0};
    float       header_text_px_{0};
    float       header_pad_x_px_{0};
    float       header_top_gap_px_{0};
    float       header_bot_gap_px_{0};
    float       content_pad_x_px_{0};
    float       content_pad_y_px_{0};
    float       title_text_px_ {0};
    float       body_text_px_  {0};
    float       close_diam_px_ {0};

    bool   open_     {false};
    bool   closing_  {false};
    float  progress_ {0.0f};

    int    active_index_  {0};
    int    hover_index_   {-1};
    int    pressed_index_ {-1};
    bool   hover_close_   {false};
    bool   pressed_close_ {false};

    std::vector<Item> items_;

    // DWrite format cache: built once per DPI, reused across frames.
    mutable ComPtr<IDWriteTextFormat> row_fmt_;
    mutable ComPtr<IDWriteTextFormat> icon_fmt_;
    mutable ComPtr<IDWriteTextFormat> header_fmt_;
    mutable ComPtr<IDWriteTextFormat> title_fmt_;
    mutable ComPtr<IDWriteTextFormat> body_fmt_;
    mutable ComPtr<IDWriteTextFormat> close_fmt_;
    mutable float built_at_row_   {0};
    mutable float built_at_icon_  {0};
    mutable float built_at_header_{0};
    mutable float built_at_title_ {0};
    mutable float built_at_body_  {0};
    mutable float built_at_close_ {0};
};

}  // namespace mactw::ui
