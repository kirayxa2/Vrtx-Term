// Borderless top-level window driven entirely by DirectComposition.
//
// Why this design works where the conventional "extend frame into client
// area" trick fails:
//
//   * The HWND is created with WS_EX_NOREDIRECTIONBITMAP. That means DWM
//     never allocates a redirection surface for it. There is literally no
//     pixel buffer the system could paint a frame onto.
//   * The Renderer creates a DirectComposition swap chain via
//     CreateSwapChainForComposition (no HWND), wraps it in a DComp visual,
//     and binds that visual to the HWND through IDCompositionTarget. From
//     this point on, the only pixels that show up where the window lives
//     are pixels we draw into the swap chain. DWM has no opinion to express.
//   * Squircle outline materialises automatically from alpha. We don't need
//     SetWindowRgn, which (a) clips to integer pixel polygons (visibly
//     jaggy at small radii) and (b) masks out hit testing as well as
//     visuals, breaking edge-resize.
//
// Resize and move are driven manually because WS_THICKFRAME is gone. We
// classify the cursor location in WM_NCHITTEST and on WM_NCLBUTTONDOWN we
// kick off a system SC_SIZE / SC_MOVE which gives us all the standard
// modifier behaviour (Aero Snap, Win+arrow tiling, etc.) without DWM ever
// drawing a frame.

#pragma once

#include "pch.h"
#include "render/Renderer.h"
#include "terminal/TerminalSession.h"
#include "ui/CaptionButton.h"
#include "ui/TrafficLights.h"

namespace mactw::window {

class BorderlessWindow {
public:
    BorderlessWindow();
    ~BorderlessWindow();

    BorderlessWindow(const BorderlessWindow&)            = delete;
    BorderlessWindow& operator=(const BorderlessWindow&) = delete;

    HWND Create(HINSTANCE hInstance, const wchar_t* title);
    HWND Hwnd() const { return hwnd_; }

    // Bind a terminal session. The window forwards keystrokes to it,
    // notifies it on resize, and asks the renderer to draw it. May be
    // null (the window then renders an empty squircle).
    void SetSession(terminal::TerminalSession* s);

    // Access the caption "more" button to attach the user-supplied click
    // handler from outside (e.g. Application::Run).
    ui::CaptionButton& GetCaptionButton() { return caption_button_; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);

    LRESULT HitTest(POINT screenPt) const;
    void OnDpiChanged(UINT newDpi, const RECT* suggested);

    // Recompute (cols, rows) for the current swap-chain size and notify the
    // session if the grid actually changed.
    void SyncPtyToSize();

    // Map a window-client pixel point (already shifted into squircle-local
    // coordinates) to a (viewRow, col) cell index. The result is clamped to
    // [-1..rows] / [-1..cols]; use IsInsideContent() to decide whether the
    // cursor is actually pointing at a cell.
    bool ContentPointToCell(int wx, int wy, int& viewRow, int& col) const;

    // Returns true when (wx, wy) is inside the cells region (below the
    // caption strip and inside the squircle).
    bool IsInsideContent(int wx, int wy) const;

    // Clipboard helpers. CopySelectionToClipboard returns true if anything
    // was placed on the clipboard.
    bool CopySelectionToClipboard();
    // Reads CF_UNICODETEXT, normalises CRLF to CR, sends to the shell.
    void PasteFromClipboard();

    HWND      hwnd_   {nullptr};
    HINSTANCE hinst_  {nullptr};
    UINT      dpi_    {96};
    bool      active_ {true};

    // Selection drag state. Tracked at the window level because we hold
    // the mouse capture while the user is dragging; the buffer just holds
    // the (anchor, head) coordinates.
    bool      selecting_{false};

    // Last (cols, rows) we told the pty. Used to avoid spamming
    // ResizePseudoConsole on every WM_SIZE that doesn't change the grid.
    int       last_cols_{0};
    int       last_rows_{0};

    render::Renderer            renderer_;
    ui::TrafficLights           traffic_;
    ui::CaptionButton           caption_button_;
    terminal::TerminalSession*  session_{nullptr};
};

}  // namespace mactw::window
