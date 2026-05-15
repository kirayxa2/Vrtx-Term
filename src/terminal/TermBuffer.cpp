#include "terminal/TermBuffer.h"

namespace mactw::terminal {

namespace {

constexpr int kTabWidth = 8;

}  // namespace

// ---------------------------------------------------------------------------

TermBuffer::TermBuffer() {
    Resize(80, 24);
}

Cell TermBuffer::BlankCell() const {
    Cell c;
    c.ch    = U' ';
    c.fg    = current_fg_;
    c.bg    = current_bg_;
    c.attrs = 0;
    return c;
}

Cell& TermBuffer::CellAt(Grid& g, int row, int col) {
    return g[static_cast<size_t>(row) * cols_ + col];
}

void TermBuffer::FillRow(Grid& g, int row, int from, int to, Cell fill) {
    if (row < 0 || row >= rows_) return;
    from = std::max(0, from);
    to   = std::min(cols_, to);
    for (int c = from; c < to; ++c) CellAt(g, row, c) = fill;
}

void TermBuffer::ClampCursor() {
    cursor_row_ = std::clamp(cursor_row_, 0, rows_ - 1);
    cursor_col_ = std::clamp(cursor_col_, 0, cols_ - 1);
}

// ---- Resize ----------------------------------------------------------------

void TermBuffer::Resize(int cols, int rows) {
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    if (cols == cols_ && rows == rows_ && !main_.empty()) return;

    auto migrate = [&](Grid& src, int oldCols, int oldRows) {
        Grid dst(static_cast<size_t>(cols) * rows, BlankCell());
        const int copyRows = std::min(oldRows, rows);
        const int copyCols = std::min(oldCols, cols);
        for (int r = 0; r < copyRows; ++r) {
            for (int c = 0; c < copyCols; ++c) {
                dst[static_cast<size_t>(r) * cols + c] =
                    src[static_cast<size_t>(r) * oldCols + c];
            }
        }
        src = std::move(dst);
    };

    const int oldCols = cols_;
    const int oldRows = rows_;

    if (main_.empty()) {
        main_.assign(static_cast<size_t>(cols) * rows, BlankCell());
        alt_.assign (static_cast<size_t>(cols) * rows, BlankCell());
    } else {
        migrate(main_, oldCols, oldRows);
        migrate(alt_,  oldCols, oldRows);
    }

    cols_          = cols;
    rows_          = rows;
    scroll_top_    = 0;
    scroll_bottom_ = rows - 1;
    ClampCursor();
    Touch();
}

// ---- Cursor ---------------------------------------------------------------

void TermBuffer::SetCursor(int row, int col) {
    cursor_row_ = row;
    cursor_col_ = col;
    ClampCursor();
    Touch();
}

void TermBuffer::SaveCursor() {
    saved_row_   = cursor_row_;
    saved_col_   = cursor_col_;
    saved_fg_    = current_fg_;
    saved_bg_    = current_bg_;
    saved_attrs_ = current_attrs_;
}

void TermBuffer::RestoreCursor() {
    cursor_row_     = saved_row_;
    cursor_col_     = saved_col_;
    current_fg_     = saved_fg_;
    current_bg_     = saved_bg_;
    current_attrs_  = saved_attrs_;
    ClampCursor();
    Touch();
}

void TermBuffer::ResetSgr() {
    current_fg_    = CellColor::MakeDefault();
    current_bg_    = CellColor::MakeDefault();
    current_attrs_ = 0;
}

// ---- Glyph emission -------------------------------------------------------

void TermBuffer::PutChar(char32_t ch) {
    if (cursor_col_ >= cols_) {
        cursor_col_ = 0;
        LineFeed();
    }
    Cell c;
    c.ch    = ch;
    c.fg    = current_fg_;
    c.bg    = current_bg_;
    c.attrs = current_attrs_;
    CellAt(*active_, cursor_row_, cursor_col_) = c;
    ++cursor_col_;
    Touch();
}

void TermBuffer::CarriageReturn() {
    cursor_col_ = 0;
    Touch();
}

void TermBuffer::LineFeed() {
    if (cursor_row_ == scroll_bottom_) {
        ScrollRegionUp(1);
    } else if (cursor_row_ < rows_ - 1) {
        ++cursor_row_;
    }
    Touch();
}

void TermBuffer::Backspace() {
    if (cursor_col_ > 0) --cursor_col_;
    Touch();
}

void TermBuffer::Tab() {
    int next = ((cursor_col_ / kTabWidth) + 1) * kTabWidth;
    cursor_col_ = std::min(next, cols_ - 1);
    Touch();
}

// ---- Erase ----------------------------------------------------------------

void TermBuffer::EraseInLine(int mode) {
    const Cell blank = BlankCell();
    switch (mode) {
        case 0: FillRow(*active_, cursor_row_, cursor_col_, cols_, blank); break;
        case 1: FillRow(*active_, cursor_row_, 0, cursor_col_ + 1, blank); break;
        case 2: FillRow(*active_, cursor_row_, 0, cols_, blank);            break;
        default: return;
    }
    Touch();
}

void TermBuffer::EraseInDisplay(int mode) {
    const Cell blank = BlankCell();
    switch (mode) {
        case 0:
            FillRow(*active_, cursor_row_, cursor_col_, cols_, blank);
            for (int r = cursor_row_ + 1; r < rows_; ++r)
                FillRow(*active_, r, 0, cols_, blank);
            break;
        case 1:
            for (int r = 0; r < cursor_row_; ++r)
                FillRow(*active_, r, 0, cols_, blank);
            FillRow(*active_, cursor_row_, 0, cursor_col_ + 1, blank);
            break;
        case 2:
        case 3:  // 3 = erase scrollback; we don't have any, so same as 2.
            for (int r = 0; r < rows_; ++r)
                FillRow(*active_, r, 0, cols_, blank);
            break;
        default: return;
    }
    Touch();
}

// ---- Insert / delete lines & chars ----------------------------------------

void TermBuffer::InsertLines(int n) {
    if (cursor_row_ < scroll_top_ || cursor_row_ > scroll_bottom_) return;
    n = std::min(std::max(1, n), scroll_bottom_ - cursor_row_ + 1);
    const Cell blank = BlankCell();

    // Move rows [cursor..bottom-n] down by n; fill cursor..cursor+n-1 with blanks.
    for (int r = scroll_bottom_; r >= cursor_row_ + n; --r) {
        for (int c = 0; c < cols_; ++c)
            CellAt(*active_, r, c) = CellAt(*active_, r - n, c);
    }
    for (int r = cursor_row_; r < cursor_row_ + n; ++r)
        FillRow(*active_, r, 0, cols_, blank);
    Touch();
}

void TermBuffer::DeleteLines(int n) {
    if (cursor_row_ < scroll_top_ || cursor_row_ > scroll_bottom_) return;
    n = std::min(std::max(1, n), scroll_bottom_ - cursor_row_ + 1);
    const Cell blank = BlankCell();

    for (int r = cursor_row_; r <= scroll_bottom_ - n; ++r) {
        for (int c = 0; c < cols_; ++c)
            CellAt(*active_, r, c) = CellAt(*active_, r + n, c);
    }
    for (int r = scroll_bottom_ - n + 1; r <= scroll_bottom_; ++r)
        FillRow(*active_, r, 0, cols_, blank);
    Touch();
}

void TermBuffer::InsertChars(int n) {
    n = std::min(std::max(1, n), cols_ - cursor_col_);
    const Cell blank = BlankCell();
    for (int c = cols_ - 1; c >= cursor_col_ + n; --c)
        CellAt(*active_, cursor_row_, c) = CellAt(*active_, cursor_row_, c - n);
    for (int c = cursor_col_; c < cursor_col_ + n; ++c)
        CellAt(*active_, cursor_row_, c) = blank;
    Touch();
}

void TermBuffer::DeleteChars(int n) {
    n = std::min(std::max(1, n), cols_ - cursor_col_);
    const Cell blank = BlankCell();
    for (int c = cursor_col_; c < cols_ - n; ++c)
        CellAt(*active_, cursor_row_, c) = CellAt(*active_, cursor_row_, c + n);
    for (int c = cols_ - n; c < cols_; ++c)
        CellAt(*active_, cursor_row_, c) = blank;
    Touch();
}

void TermBuffer::EraseChars(int n) {
    n = std::min(std::max(1, n), cols_ - cursor_col_);
    const Cell blank = BlankCell();
    for (int c = cursor_col_; c < cursor_col_ + n; ++c)
        CellAt(*active_, cursor_row_, c) = blank;
    Touch();
}

// ---- Scroll ---------------------------------------------------------------

void TermBuffer::SetScrollRegion(int top, int bottom) {
    top    = std::clamp(top,    0, rows_ - 1);
    bottom = std::clamp(bottom, 0, rows_ - 1);
    if (top >= bottom) { top = 0; bottom = rows_ - 1; }
    scroll_top_    = top;
    scroll_bottom_ = bottom;
    cursor_row_    = top;
    cursor_col_    = 0;
    Touch();
}

void TermBuffer::ScrollRegionUp(int n) {
    n = std::min(std::max(1, n), scroll_bottom_ - scroll_top_ + 1);
    const Cell blank = BlankCell();
    for (int r = scroll_top_; r <= scroll_bottom_ - n; ++r) {
        for (int c = 0; c < cols_; ++c)
            CellAt(*active_, r, c) = CellAt(*active_, r + n, c);
    }
    for (int r = scroll_bottom_ - n + 1; r <= scroll_bottom_; ++r)
        FillRow(*active_, r, 0, cols_, blank);
}

void TermBuffer::ScrollRegionDown(int n) {
    n = std::min(std::max(1, n), scroll_bottom_ - scroll_top_ + 1);
    const Cell blank = BlankCell();
    for (int r = scroll_bottom_; r >= scroll_top_ + n; --r) {
        for (int c = 0; c < cols_; ++c)
            CellAt(*active_, r, c) = CellAt(*active_, r - n, c);
    }
    for (int r = scroll_top_; r < scroll_top_ + n; ++r)
        FillRow(*active_, r, 0, cols_, blank);
}

void TermBuffer::ScrollUp(int n)   { ScrollRegionUp(n);   Touch(); }
void TermBuffer::ScrollDown(int n) { ScrollRegionDown(n); Touch(); }

// ---- Alt screen -----------------------------------------------------------

void TermBuffer::SwitchToAltScreen() {
    if (alt_active_) return;
    // Clear alt on entry to mimic xterm's smcup behaviour.
    const Cell blank = BlankCell();
    for (auto& c : alt_) c = blank;
    active_ = &alt_;
    alt_active_ = true;
    Touch();
}

void TermBuffer::SwitchToMainScreen() {
    if (!alt_active_) return;
    active_ = &main_;
    alt_active_ = false;
    Touch();
}

}  // namespace mactw::terminal
