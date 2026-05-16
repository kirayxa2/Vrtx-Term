// In-window Settings sheet rendered through D2D - the Vrtx Term
// In-window Settings sheet rendered through D2D.
//
// Layout (no scrim, no blur — the sheet **replaces** the terminal area):
//
//   +--squircle (caption strip stays at top)---------------------------+
//   | TL  TL  TL                                              [Done]   |
//   |                                                                  |
//   |  +--sidebar pill---------+                                       |
//   |  |  General              |    Title 22pt                         |
//   |  |  Appearance           |    +--card------------------+         |
//   |  |  Prompt               |    |  Row label    value  > |         |
//   |  |  Terminal             |    |---------------------- |         |
//   |  |  About                |    |  Row label    [toggl] |         |
//   |  +-----------------------+    +-----------------------+         |
//   +------------------------------------------------------------------+
//
//   * Sidebar is a separate floating rounded pill with the same
//     corner radius and 1pt hairline as the window itself.
//   * Content area is the bare squircle to the right of the pill.
//     Each "Section" inside the active item renders as one rounded
//     "card" - a translucent panel with rows and 1pt hairline
//     separators between them, exactly like the Settings sheet.
//   * Done is a system-blue pill in the top-right of the caption
//     strip (it replaces the chrome chevron-button while open).

#pragma once

#include "pch.h"
#include "theme/AppTheme.h"

namespace vrtx::ui {

class SettingsView {
public:
    using ItemHandler = std::function<void()>;
    using DoneHandler = std::function<void()>;
    using ToggleHandler = std::function<void(bool& /*new_state, mutable*/)>;

    // ---- Row model -------------------------------------------------------
    //
    // Grouped-table cells come in a small set of flavours.
    // Plain   - just a label (and optional chevron). on_pick fires
    //           when clicked.
    // Value   - label + trailing string (e.g. "Dark") + optional
    //           chevron.
    // Toggle  - label + on/off switch on the right. Click anywhere on the
    //           row flips it.
    enum class RowKind { Plain, Value, Toggle };

    struct Row {
        RowKind        kind          {RowKind::Plain};
        std::wstring   label;
        std::wstring   value;          // for Value rows
        bool           toggle_state   {false};   // for Toggle rows
        bool           show_chevron   {false};   // Plain / Value
        bool           enabled        {true};
        ItemHandler    on_pick;        // Plain/Value: clicked
        ToggleHandler  on_toggle;      // Toggle: state changed
    };

    // A Section is one rounded card in the content pane. Optional
    // small header above ("APPEARANCE") and optional small footer
    // below ("This applies to the entire app.").
    struct Section {
        std::wstring     header;
        std::wstring     footer;
        std::vector<Row> rows;
    };

    // A sidebar entry - one line in the left pill with its tile,
    // label, and the list of sections it shows on the right.
    struct Item {
        std::wstring         glyph;       // single grapheme drawn on the tile
        std::wstring         label;       // localised
        std::wstring         title;       // shown as the main pane title
        theme::Color         tile_color;  // tile background (saturated)
        std::vector<Section> sections;    // content cards for this category
        ItemHandler          on_pick;     // optional hook when row is clicked
    };

    void AddItem(Item it) { items_.push_back(std::move(it)); }

    // Mutate the rows of an existing item (e.g. to flip toggle state
    // after the host has applied the change). Indices are item, section,
    // row. Out-of-range silently ignored.
    Row* RowAt(int item_idx, int section_idx, int row_idx) {
        if (item_idx < 0 || item_idx >= static_cast<int>(items_.size()))
            return nullptr;
        auto& s = items_[item_idx].sections;
        if (section_idx < 0 || section_idx >= static_cast<int>(s.size()))
            return nullptr;
        auto& r = s[section_idx].rows;
        if (row_idx < 0 || row_idx >= static_cast<int>(r.size()))
            return nullptr;
        return &r[row_idx];
    }

    // Lay out the sheet inside the squircle (squircle-local px).
    // captionHeightPx is the bottom of the caption strip.
    void UpdateLayout(D2D1_RECT_F squircleRect, float captionHeightPx,
                      UINT dpi);

    bool IsOpen()      const { return open_; }
    bool IsClosing()   const { return closing_; }

    void Show()           { open_ = true;  closing_ = false;
                             hover_index_ = -1; pressed_index_ = -1;
                             hover_done_ = false; pressed_done_ = false;
                             hover_row_path_ = {-1,-1,-1};
                             pressed_row_path_ = {-1,-1,-1}; }
    void RequestClose()   { closing_ = true; }
    void SetOpen(bool v) {
        open_ = v;
        if (!v) {
            closing_ = false;
            hover_index_ = -1;
            pressed_index_ = -1;
            hover_done_ = false;
            pressed_done_ = false;
            hover_row_path_ = {-1,-1,-1};
            pressed_row_path_ = {-1,-1,-1};
        }
    }
    void SetProgress(float p) { progress_ = std::clamp(p, 0.0f, 1.0f); }
    float Progress() const    { return progress_; }

    // Done button bounds in squircle-local px.
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
    int  SidebarRowAt(int x, int y) const;
    bool DoneAt(int x, int y) const;

    // (item, section, row) tuple; -1 means "not in any row".
    struct RowPath { int item{-1}; int section{-1}; int row{-1}; };
    RowPath ContentRowAt(int x, int y) const;

    // Compute the rect of a content row at (section, row) for the
    // currently active item.
    bool ContentRowRect(int section, int row, D2D1_RECT_F& out) const;

    // Layout cache (squircle-local px).
    D2D1_RECT_F sidebar_       {};   // floating sidebar pill
    D2D1_RECT_F content_       {};   // bare squircle area for the pane
    D2D1_RECT_F done_btn_      {};   // top-right Done pill

    // Sidebar metrics (px).
    float       sidebar_radius_px_  {0};
    float       sidebar_border_px_  {0};
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

    // Content metrics (px).
    float       content_pad_x_px_   {0};
    float       content_pad_top_px_ {0};
    float       title_text_px_      {0};
    float       title_bot_gap_px_   {0};

    // Card metrics (px).
    float       card_radius_px_     {0};
    float       card_spacing_px_    {0};
    float       card_pad_x_px_      {0};
    float       card_row_h_px_      {0};
    float       card_sep_inset_px_  {0};
    float       card_sep_w_px_      {0};
    float       section_header_px_  {0};
    float       section_header_gap_px_ {0};
    float       row_label_px_       {0};
    float       row_value_px_       {0};
    float       row_chev_px_        {0};
    float       row_chev_gap_px_    {0};
    float       row_end_pad_px_     {0};
    float       toggle_w_px_        {0};
    float       toggle_h_px_        {0};
    float       toggle_knob_px_     {0};
    float       toggle_knob_inset_px_ {0};
    float       footer_text_px_     {0};
    float       footer_top_gap_px_  {0};
    float       footer_bot_gap_px_  {0};

    float       done_text_px_       {0};

    bool   open_     {false};
    bool   closing_  {false};
    float  progress_ {0.0f};

    int    active_index_  {0};
    int    hover_index_   {-1};
    int    pressed_index_ {-1};
    bool   hover_done_    {false};
    bool   pressed_done_  {false};

    RowPath hover_row_path_   {-1,-1,-1};
    RowPath pressed_row_path_ {-1,-1,-1};

    std::vector<Item> items_;
    DoneHandler       on_done_;
    std::wstring      done_label_{L"Done"};

    // DWrite format cache: built once per DPI, reused across frames.
    mutable ComPtr<IDWriteTextFormat> row_fmt_;
    mutable ComPtr<IDWriteTextFormat> tile_fmt_;
    mutable ComPtr<IDWriteTextFormat> title_fmt_;
    mutable ComPtr<IDWriteTextFormat> done_fmt_;
    mutable ComPtr<IDWriteTextFormat> header_fmt_;
    mutable ComPtr<IDWriteTextFormat> label_fmt_;
    mutable ComPtr<IDWriteTextFormat> value_fmt_;
    mutable ComPtr<IDWriteTextFormat> chev_fmt_;
    mutable ComPtr<IDWriteTextFormat> footer_fmt_;
    mutable float built_at_row_    {0};
    mutable float built_at_tile_   {0};
    mutable float built_at_title_  {0};
    mutable float built_at_done_   {0};
    mutable float built_at_header_ {0};
    mutable float built_at_label_  {0};
    mutable float built_at_value_  {0};
    mutable float built_at_chev_   {0};
    mutable float built_at_footer_ {0};
};

}  // namespace vrtx::ui
