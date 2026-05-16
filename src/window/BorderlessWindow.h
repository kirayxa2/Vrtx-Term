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
#include "ui/AppAlert.h"
#include "ui/CaptionButton.h"
#include "ui/CaptionMenu.h"
#include "ui/SettingsView.h"
#include "ui/TrafficLights.h"
#include "ui/TabBar.h"

namespace vrtx::window {

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
    ui::TabBar&      GetTabBar()        { return tab_bar_; }
    ui::CaptionButton& GetCaptionButton() { return caption_button_; }
    ui::CaptionMenu&   GetCaptionMenu()   { return caption_menu_; }
    ui::AppAlert&      GetAppAlert()      { return app_alert_; }
    ui::SettingsView&  GetSettings()      { return settings_; }

    // Open the in-window alert with the given strings and animate it in.
    // This replaces any direct MessageBoxW calls so all dialogs stay in
    // our own chrome.
    void ShowAlert(std::wstring title,
                   std::wstring message,
                   std::wstring buttonText);

    // Open / close the Settings sheet with animation.
    void ShowSettings();
    void HideSettings();

    // Re-layout the tab bar (call after SetTabs from outside).
    void RelayoutTabBar();

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
    ui::TabBar                  tab_bar_;
    ui::CaptionButton           caption_button_;
    ui::CaptionMenu             caption_menu_;
    ui::AppAlert                app_alert_;
    ui::SettingsView            settings_;
    terminal::TerminalSession*  session_{nullptr};

    // ---- Caption menu animation ---------------------------------------
    //
    // The menu's open/close transition is driven by a Win32 timer set up
    // when the user toggles the button. `menu_anim_target_` is what the
    // animation is heading toward (0 = closed, 1 = open). `menu_anim_t_`
    // is the current normalised position. When `menu_anim_t_` reaches the
    // target the timer is killed.
    UINT_PTR menu_timer_id_   {0};      // 0 means no timer running
    DWORD    menu_anim_start_ {0};      // GetTickCount() at last toggle
    float    menu_anim_from_  {0.0f};   // value at the moment of toggle
    float    menu_anim_target_{0.0f};   // 0 or 1
    float    menu_anim_t_     {0.0f};   // current eased position [0..1]

    void ToggleMenu();
    void StartMenuAnimation(float target);
    void OnMenuTimer();
    void RelayoutCaptionMenu();

    // ---- App alert animation ------------------------------------------
    //
    // Mirrors the menu animation logic; we keep the timer separate so an
    // alert can open over the caption menu and vice-versa without their
    // animations cross-contaminating.
    UINT_PTR alert_timer_id_   {0};
    DWORD    alert_anim_start_ {0};
    float    alert_anim_from_  {0.0f};
    float    alert_anim_target_{0.0f};
    float    alert_anim_t_     {0.0f};

    void StartAlertAnimation(float target);
    void OnAlertTimer();
    void RelayoutAppAlert();
    void DismissAlert();

    // ---- Settings sheet animation -------------------------------------
    UINT_PTR settings_timer_id_   {0};
    DWORD    settings_anim_start_ {0};
    float    settings_anim_from_  {0.0f};
    float    settings_anim_target_{0.0f};
    float    settings_anim_t_     {0.0f};

    void StartSettingsAnimation(float target);
    void OnSettingsTimer();
    void RelayoutSettings();

    // ---- Traffic lights glyph-fade animation -------------------------
    UINT_PTR traffic_timer_id_{0};

    void OnTrafficTimer();

    // ---- Cursor blink ---------------------------------------------------
    // macOS cursor blink: 530ms on / 530ms off.
    UINT_PTR cursor_blink_timer_id_{0};
    bool     cursor_visible_{true};   // current blink state

    void OnCursorBlinkTimer();
    void ResetCursorBlink();  // call on any keystroke to make cursor visible
};

}  // namespace vrtx::window
