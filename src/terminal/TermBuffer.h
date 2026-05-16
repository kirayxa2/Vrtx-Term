// TermBuffer - the in-memory model of the terminal grid.
//
// Owns a 2D array of `Cell`s, the cursor position, current SGR state, the
// scroll region, an alternate screen buffer (used by full-screen apps like
// vim/htop via DECSET 1049), a ring of scrollback lines that have moved off
// the top of the main screen, and a selection range expressed in absolute
// row coordinates so that drag-selecting works seamlessly across the
// scrollback boundary.
//
// Threading: a single `std::mutex` guards the entire object. The reader
// thread takes the lock when applying parsed input; the renderer takes it
// each frame when reading cells out. The window thread takes it when
// updating the selection or moving the viewport. Contention is tiny (one
// short critical section per pty chunk / per frame / per mouse event).
//
// Out of scope for the MVP:
//   * Wide (CJK) characters - we treat every codepoint as 1 cell.
//   * Combining marks - dropped on the floor.
//   * Reflow on resize - scrollback rows keep their original column count.
//
// These can land in follow-up PRs without changing the public surface.

#pragma once

#include "pch.h"

#include <deque>
#include <mutex>
#include <string>

namespace vrtx::terminal {

// ---- Color reference inside a cell ----------------------------------------

// Three encoding modes:
//   Default  - "no override"; renderer uses palette.terminalFg / terminalBg.
//   Indexed  - one of 16 ANSI palette slots; `index` holds 0..15. Slots
//              16..255 (xterm 256-color) also use this mode if we ever wire
//              them up.
//   Rgb      - direct truecolor; `r`,`g`,`b` are 8-bit.
struct CellColor {
    enum class Mode : uint8_t { Default = 0, Indexed = 1, Rgb = 2 };

    Mode    mode{Mode::Default};
    uint8_t index{0};
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};

    static constexpr CellColor MakeDefault()              { return {Mode::Default, 0, 0, 0, 0}; }
    static constexpr CellColor MakeIndexed(uint8_t idx)   { return {Mode::Indexed, idx, 0, 0, 0}; }
    static constexpr CellColor MakeRgb(uint8_t R, uint8_t G, uint8_t B)
                                                          { return {Mode::Rgb, 0, R, G, B}; }
};

// ---- Cell attributes -------------------------------------------------------

namespace attr {
inline constexpr uint16_t kBold      = 1u << 0;
inline constexpr uint16_t kFaint     = 1u << 1;
inline constexpr uint16_t kItalic    = 1u << 2;
inline constexpr uint16_t kUnderline = 1u << 3;
inline constexpr uint16_t kReverse   = 1u << 4;
inline constexpr uint16_t kStrike    = 1u << 5;
}  // namespace attr

// One grid cell. 16 bytes on x64.
struct Cell {
    char32_t  ch{U' '};
    CellColor fg{};
    CellColor bg{};
    uint16_t  attrs{0};
    uint16_t  _pad{0};
};

// ---- Selection ------------------------------------------------------------

// A single point in the buffer addressed by absolute row + column. Absolute
// row indexing convention:
//   row in [0, ScrollbackLines())                              -> scrollback
//   row in [ScrollbackLines(), ScrollbackLines() + Rows())     -> live grid
//
// As scrollback older than `kMaxScrollback` is discarded the absolute row
// numbers shift downward; TermBuffer takes care to keep any active
// selection clamped so this is invisible to callers.
struct SelPoint {
    int64_t row{0};
    int     col{0};
};

inline bool operator<(const SelPoint& a, const SelPoint& b) {
    return (a.row == b.row) ? (a.col < b.col) : (a.row < b.row);
}
inline bool operator==(const SelPoint& a, const SelPoint& b) {
    return a.row == b.row && a.col == b.col;
}

// ---- TermBuffer -----------------------------------------------------------

class TermBuffer {
public:
    TermBuffer();

    // Geometry ---------------------------------------------------------------
    //
    // Resize to `cols` x `rows`. Content from the old grid is preserved at
    // the same (col, row) where it still fits; otherwise it is discarded.
    // Cursor and scroll region are clamped into the new bounds. Affects
    // both main and alt screens. Scrollback rows are kept verbatim and
    // padded / clipped at render time when they have a different width.
    void Resize(int cols, int rows);

    int  Cols() const { return cols_; }
    int  Rows() const { return rows_; }

    // Cursor -----------------------------------------------------------------
    int  CursorRow() const { return cursor_row_; }
    int  CursorCol() const { return cursor_col_; }
    void SetCursor(int row, int col);

    // Cursor save / restore (DECSC / DECRC + the alt-screen variant 1048).
    void SaveCursor();
    void RestoreCursor();

    // SGR --------------------------------------------------------------------
    void SetFg(CellColor c)    { current_fg_ = c; }
    void SetBg(CellColor c)    { current_bg_ = c; }
    void SetAttr(uint16_t a)   { current_attrs_ |= a; }
    void ClearAttr(uint16_t a) { current_attrs_ &= static_cast<uint16_t>(~a); }
    void ResetSgr();

    // Glyph emission --------------------------------------------------------
    //
    // Insert one codepoint at the cursor. Wraps to the next line when at
    // the right margin, scrolls when at the bottom of the scroll region.
    void PutChar(char32_t ch);

    // Control characters
    void CarriageReturn();   // \r
    void LineFeed();         // \n - moves down, scrolls if needed
    void Backspace();        // \b
    void Tab();              // \t - 8-column stops

    // Erase operations (CSI J / K) ------------------------------------------
    //
    // mode: 0 = cursor..end, 1 = start..cursor, 2 = all.
    void EraseInLine(int mode);
    void EraseInDisplay(int mode);

    // CSI L / M (insert / delete N lines at cursor row, within scroll region).
    void InsertLines(int n);
    void DeleteLines(int n);

    // CSI @ / P (insert / delete N chars at cursor on the current line).
    void InsertChars(int n);
    void DeleteChars(int n);

    // CSI X (erase N chars at cursor, no shift).
    void EraseChars(int n);

    // Scroll region ---------------------------------------------------------
    void SetScrollRegion(int top, int bottom);  // 0-based, inclusive
    int  ScrollTop()    const { return scroll_top_; }
    int  ScrollBottom() const { return scroll_bottom_; }

    // Direct scroll within the current region.
    void ScrollUp(int n);     // CSI S - lines move up, blanks at bottom
    void ScrollDown(int n);   // CSI T

    // Alt screen ------------------------------------------------------------
    bool OnAltScreen() const { return alt_active_; }
    void SwitchToAltScreen();
    void SwitchToMainScreen();

    // Renderer access -------------------------------------------------------
    //
    // The renderer must hold Lock() for the entire duration of reading.
    std::mutex& Lock() const { return mutex_; }

    // Bumped on every mutation; the UI uses this to skip needless repaints.
    uint64_t Generation() const { return generation_; }

    // ---- Scrollback / viewport -------------------------------------------
    //
    // Scrollback grows when the main screen scrolls and `scroll_top_ == 0`;
    // archived rows live in `scrollback_`. Alt-screen scrolling is volatile
    // (xterm's historical behaviour - vim/htop do not contribute history).
    //
    // The viewport is the slice the user is currently looking at. Offset 0
    // means "the live grid" (last `rows_` lines); a positive offset shifts
    // the slice upward into the scrollback. The offset never exceeds the
    // scrollback length.

    int  ScrollbackLines() const { return static_cast<int>(scrollback_.size()); }
    int  ViewportOffset()  const { return viewport_offset_; }
    int  MaxViewportOffset() const { return ScrollbackLines(); }

    // Move the viewport by `delta` lines (positive = back in time).
    // Returns true if the offset actually changed.
    bool ScrollViewport(int delta);

    // Reset viewport to the live grid. Called whenever the user types so
    // they can see the response to their input.
    void SnapToBottom();

    // Copy `Cols()` cells of viewport row `viewRow` (0..Rows()-1) into dst.
    // dst must point to at least Cols() Cell slots.
    void GetViewportRow(int viewRow, Cell* dst) const;

    // Convert a viewport-local row into an absolute row in this buffer.
    // Useful for selection handling.
    int64_t ViewportRowToAbs(int viewRow) const;

    // ---- Selection -------------------------------------------------------
    //
    // The selection is two SelPoints in absolute coordinates. When clamped
    // outside the live buffer (after old scrollback gets evicted) the
    // selection is silently truncated or cleared.

    bool HasSelection() const { return has_selection_; }
    void ClearSelection();

    // Begin a fresh selection at `p`. Replaces any prior selection.
    void StartSelection(SelPoint p);

    // Drag the head of the in-progress selection to `p`.
    void UpdateSelection(SelPoint p);

    // Returns true if the cell at (absRow, col) is currently selected.
    bool IsCellSelected(int64_t absRow, int col) const;

    // Build a UTF-8 string of the current selection. Trailing whitespace
    // on each line is trimmed; lines are separated by '\n'.
    std::string SelectionText() const;

private:
    using Grid = std::vector<Cell>;

    void   ClampCursor();
    Cell&  CellAt(Grid& g, int row, int col);
    const Cell& CellAt(const Grid& g, int row, int col) const;
    void   FillRow(Grid& g, int row, int colFrom, int colTo, Cell fill);
    void   ScrollRegionUp(int n);
    void   ScrollRegionDown(int n);
    Cell   BlankCell() const;
    void   Touch() { ++generation_; }

    // Push n top rows of the active grid into scrollback. Caller must have
    // verified that we're on main screen and scroll_top_ == 0.
    void   ArchiveTopRowsToScrollback(int n);

    // Trim scrollback ring; also shifts selection coordinates so active
    // selection points remain valid against the live deque.
    void   TrimScrollback();

    // Normalise selection so sel_anchor_ <= sel_head_.
    void   GetNormalisedSelection(SelPoint& a, SelPoint& b) const;

    mutable std::mutex mutex_;

    int cols_{80};
    int rows_{24};

    int cursor_row_{0};
    int cursor_col_{0};

    int saved_row_{0};
    int saved_col_{0};
    CellColor saved_fg_{};
    CellColor saved_bg_{};
    uint16_t  saved_attrs_{0};

    int scroll_top_{0};
    int scroll_bottom_{23};  // inclusive

    CellColor current_fg_{};
    CellColor current_bg_{};
    uint16_t  current_attrs_{0};

    Grid main_;
    Grid alt_;
    Grid* active_{&main_};
    bool  alt_active_{false};

    // Scrollback ring. Each entry is one row of cells of arbitrary length
    // (its width at the moment it was archived). The renderer pads / clips
    // to the current Cols() at draw time.
    std::deque<std::vector<Cell>> scrollback_;
    static constexpr int kMaxScrollback = 5000;

    // 0 = looking at the live bottom; positive = scrolled up by N lines.
    int viewport_offset_{0};

    // Selection.
    bool     has_selection_{false};
    SelPoint sel_anchor_{};
    SelPoint sel_head_{};

    uint64_t generation_{0};
};

}  // namespace vrtx::terminal
