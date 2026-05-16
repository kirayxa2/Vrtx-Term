#include "terminal/TermBuffer.h"

#include <algorithm>

namespace vrtx::terminal {

namespace {

constexpr int kTabWidth = 8;

// Encode one Unicode codepoint as UTF-8 into `out`. Returns bytes written.
int EncodeUtf8(char32_t cp, char* out) {
    if (cp < 0x80) {
        out[0] = static_cast<char>(cp);
        return 1;
    } else if (cp < 0x800) {
        out[0] = static_cast<char>(0xC0 | (cp >> 6));
        out[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (cp >> 12));
        out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = static_cast<char>(0xF0 | (cp >> 18));
        out[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (cp & 0x3F));
        return 4;
    }
}

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

const Cell& TermBuffer::CellAt(const Grid& g, int row, int col) const {
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

    // A geometry change resets the viewport to the bottom and drops any
    // active selection: the user almost certainly wants to see the current
    // prompt, and the selection's column indices may no longer make sense.
    viewport_offset_ = 0;
    ClearSelection();
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
        case 3:
            // mode 3 = "also clear scrollback"; accommodate it.
            for (int r = 0; r < rows_; ++r)
                FillRow(*active_, r, 0, cols_, blank);
            if (mode == 3) {
                scrollback_.clear();
                ClearSelection();
                viewport_offset_ = 0;
            }
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

void TermBuffer::ArchiveTopRowsToScrollback(int n) {
    n = std::min(std::max(1, n), rows_);
    for (int r = 0; r < n; ++r) {
        std::vector<Cell> line(cols_);
        for (int c = 0; c < cols_; ++c) {
            line[c] = CellAt(*active_, r, c);
        }
        scrollback_.push_back(std::move(line));
    }
    TrimScrollback();
}

void TermBuffer::TrimScrollback() {
    int removed = 0;
    while (static_cast<int>(scrollback_.size()) > kMaxScrollback) {
        scrollback_.pop_front();
        ++removed;
    }
    if (removed == 0) return;

    // Shift any active selection points up by `removed` so they keep
    // pointing at the same logical content. If either end would fall
    // below row 0, the content it referred to has been evicted, so the
    // selection no longer makes sense - drop it. Doing so is safer than
    // silently re-anchoring it onto unrelated rows.
    if (has_selection_) {
        sel_anchor_.row -= removed;
        sel_head_.row   -= removed;
        if (sel_anchor_.row < 0 || sel_head_.row < 0) {
            ClearSelection();
        }
    }

    // The viewport offset is measured in "lines back from the live grid",
    // not absolute rows, so it is unaffected by the trim.
}

void TermBuffer::ScrollRegionUp(int n) {
    n = std::min(std::max(1, n), scroll_bottom_ - scroll_top_ + 1);

    // Archive the top rows of the *full* scroll region only when the
    // region covers the whole screen (the typical case for normal output).
    // Custom scroll regions (vim status line tricks) are NOT archived.
    if (!alt_active_ && scroll_top_ == 0 && scroll_bottom_ == rows_ - 1) {
        ArchiveTopRowsToScrollback(n);
    }

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
    const Cell blank = BlankCell();
    for (auto& c : alt_) c = blank;
    active_ = &alt_;
    alt_active_ = true;
    // Alt-screen apps own the viewport.
    viewport_offset_ = 0;
    ClearSelection();
    Touch();
}

void TermBuffer::SwitchToMainScreen() {
    if (!alt_active_) return;
    active_ = &main_;
    alt_active_ = false;
    viewport_offset_ = 0;
    ClearSelection();
    Touch();
}

// ---- Viewport / scrollback ------------------------------------------------

bool TermBuffer::ScrollViewport(int delta) {
    const int newOffset = std::clamp(viewport_offset_ + delta,
                                     0, MaxViewportOffset());
    if (newOffset == viewport_offset_) return false;
    viewport_offset_ = newOffset;
    Touch();
    return true;
}

void TermBuffer::SnapToBottom() {
    if (viewport_offset_ != 0) {
        viewport_offset_ = 0;
        Touch();
    }
}

int64_t TermBuffer::ViewportRowToAbs(int viewRow) const {
    // Live grid bottom row's absolute index is scrollback_.size() + rows_ - 1.
    // Top of viewport corresponds to abs row
    //     (scrollback + rows - 1) - (viewport_offset + (rows - 1 - viewRow))
    //   = scrollback + viewRow - viewport_offset.
    return static_cast<int64_t>(scrollback_.size()) + viewRow - viewport_offset_;
}

void TermBuffer::GetViewportRow(int viewRow, Cell* dst) const {
    Cell blank = BlankCell();
    blank.ch  = U' ';
    blank.fg  = CellColor::MakeDefault();
    blank.bg  = CellColor::MakeDefault();
    blank.attrs = 0;

    const int64_t absRow = ViewportRowToAbs(viewRow);
    const int64_t sbSize = static_cast<int64_t>(scrollback_.size());

    if (absRow < 0 || absRow >= sbSize + rows_) {
        for (int c = 0; c < cols_; ++c) dst[c] = blank;
        return;
    }

    if (absRow < sbSize) {
        const auto& line = scrollback_[static_cast<size_t>(absRow)];
        const int copy = std::min(static_cast<int>(line.size()), cols_);
        for (int c = 0; c < copy;  ++c) dst[c] = line[c];
        for (int c = copy; c < cols_; ++c) dst[c] = blank;
    } else {
        const int liveRow = static_cast<int>(absRow - sbSize);
        for (int c = 0; c < cols_; ++c)
            dst[c] = CellAt(*active_, liveRow, c);
    }
}

// ---- Selection ------------------------------------------------------------

void TermBuffer::ClearSelection() {
    if (!has_selection_) return;
    has_selection_ = false;
    Touch();
}

void TermBuffer::StartSelection(SelPoint p) {
    has_selection_ = true;
    sel_anchor_ = p;
    sel_head_   = p;
    Touch();
}

void TermBuffer::UpdateSelection(SelPoint p) {
    if (!has_selection_) {
        StartSelection(p);
        return;
    }
    if (!(p == sel_head_)) {
        sel_head_ = p;
        Touch();
    }
}

void TermBuffer::GetNormalisedSelection(SelPoint& a, SelPoint& b) const {
    a = sel_anchor_;
    b = sel_head_;
    if (b < a) std::swap(a, b);
}

bool TermBuffer::IsCellSelected(int64_t absRow, int col) const {
    if (!has_selection_) return false;
    if (sel_anchor_ == sel_head_) return false;

    SelPoint a{}, b{};
    GetNormalisedSelection(a, b);

    // Treat selection as a range [a, b) in row-major order, where row
    // boundaries take precedence over column boundaries.
    if (absRow < a.row || absRow > b.row) return false;
    if (a.row == b.row) {
        return col >= a.col && col < b.col;
    }
    if (absRow == a.row) return col >= a.col;
    if (absRow == b.row) return col <  b.col;
    return true;  // any row strictly between
}

std::string TermBuffer::SelectionText() const {
    if (!has_selection_ || sel_anchor_ == sel_head_) return {};

    SelPoint a{}, b{};
    GetNormalisedSelection(a, b);

    const int64_t sbSize = static_cast<int64_t>(scrollback_.size());
    std::string out;
    out.reserve(static_cast<size_t>((b.row - a.row + 1) * cols_));

    std::vector<Cell> rowBuf(static_cast<size_t>(cols_));

    for (int64_t r = a.row; r <= b.row; ++r) {
        if (r < 0 || r >= sbSize + rows_) continue;

        // Fetch row content: scrollback or live grid.
        const Cell* src = nullptr;
        int srcWidth = cols_;
        if (r < sbSize) {
            const auto& line = scrollback_[static_cast<size_t>(r)];
            src      = line.data();
            srcWidth = static_cast<int>(line.size());
        } else {
            const int liveRow = static_cast<int>(r - sbSize);
            for (int c = 0; c < cols_; ++c) {
                rowBuf[c] = CellAt(*active_, liveRow, c);
            }
            src = rowBuf.data();
            srcWidth = cols_;
        }

        const int colFrom = (r == a.row) ? a.col : 0;
        const int colTo   = (r == b.row) ? b.col : cols_;
        const int from = std::max(0, colFrom);
        const int to   = std::min(srcWidth, colTo);

        // Find the index of the last non-space cell so we can right-trim.
        int lastNonSpace = from - 1;
        for (int c = from; c < to; ++c) {
            if (src[c].ch != 0 && src[c].ch != U' ') lastNonSpace = c;
        }
        const int emitTo = lastNonSpace + 1;

        char ub[4];
        for (int c = from; c < emitTo; ++c) {
            char32_t cp = src[c].ch;
            if (cp == 0) cp = U' ';
            const int n = EncodeUtf8(cp, ub);
            out.append(ub, ub + n);
        }
        if (r != b.row) out.push_back('\n');
    }
    return out;
}

}  // namespace vrtx::terminal
