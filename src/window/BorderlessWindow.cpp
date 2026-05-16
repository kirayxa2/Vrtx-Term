#include "window/BorderlessWindow.h"

#include "terminal/TermInput.h"
#include "theme/TahoeTheme.h"
#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

constexpr wchar_t kClassName[] = L"MacTermWin.BorderlessWindow";

// Custom message we post from the pty reader thread to nudge the UI to
// repaint. WM_APP is the documented base for app-private messages.
constexpr UINT WM_APP_PTY_DIRTY = WM_APP + 1;

// Number of grid lines per mouse-wheel detent. Three is the GTK / VS Code
// default and matches Windows Terminal.
constexpr int kWheelLinesPerDetent = 3;

// Caption-menu animation: ~16ms tick (≈60Hz) and a unique timer id.
constexpr UINT_PTR kMenuTimerId      = 0x4D54u;   // 'MT'
constexpr UINT     kMenuTimerPeriod  = 16;

// App-alert animation timer id. Shares the same 60Hz cadence; the IDs
// are distinct so the WM_TIMER handler can dispatch correctly.
constexpr UINT_PTR kAlertTimerId     = 0x4154u;   // 'AT'
constexpr UINT     kAlertTimerPeriod = 16;

// Settings sheet animation timer id.
constexpr UINT_PTR kSettingsTimerId     = 0x5354u;   // 'ST'
constexpr UINT     kSettingsTimerPeriod = 16;

// Get per-monitor DPI; falls back to 96 only on pre-1607 systems.
UINT GetWindowDpiSafe(HWND hwnd) {
    using PFN = UINT(WINAPI*)(HWND);
    static PFN fn = []() -> PFN {
        const HMODULE u = ::GetModuleHandleW(L"user32.dll");
        return u ? reinterpret_cast<PFN>(::GetProcAddress(u, "GetDpiForWindow"))
                 : nullptr;
    }();
    return fn ? fn(hwnd) : 96u;
}

void Trace(const char* msg) {
    wchar_t tempDir[MAX_PATH] = {};
    if (!::GetTempPathW(MAX_PATH, tempDir)) return;
    wchar_t path[MAX_PATH] = {};
    std::swprintf(path, MAX_PATH, L"%smactermwin.log", tempDir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        std::fprintf(f, "  win: %s\n", msg);
        std::fclose(f);
    }
}

}  // namespace

// ---------------------------------------------------------------------------

BorderlessWindow::BorderlessWindow() = default;

BorderlessWindow::~BorderlessWindow() {
    if (hwnd_) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    ::UnregisterClassW(kClassName, hinst_);
}

LRESULT CALLBACK BorderlessWindow::StaticWndProc(HWND hwnd, UINT msg,
                                                 WPARAM wp, LPARAM lp) {
    BorderlessWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<BorderlessWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<BorderlessWindow*>(
            ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->WndProc(msg, wp, lp);
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

HWND BorderlessWindow::Create(HINSTANCE hInstance, const wchar_t* title) {
    hinst_ = hInstance;

    Trace("Create() entered");

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &BorderlessWindow::StaticWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    ::RegisterClassExW(&wc);
    Trace("class registered");

    dpi_ = ::GetDpiForSystem();

    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
    const int wPx = theme::ToPxInt(theme::kDefaultWindowWidth,  dpi_) + 2 * marginPx;
    const int hPx = theme::ToPxInt(theme::kDefaultWindowHeight, dpi_) + 2 * marginPx;

    constexpr DWORD style   = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                              WS_CLIPCHILDREN;
    constexpr DWORD exStyle = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;

    HWND hwnd = ::CreateWindowExW(
        exStyle, kClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, wPx, hPx,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) {
        throw std::runtime_error("CreateWindowExW failed");
    }
    Trace("hwnd created");

    dpi_ = GetWindowDpiSafe(hwnd);

    renderer_.SetDpi(dpi_);
    Trace("calling renderer init");
    renderer_.Initialize(hwnd);
    Trace("renderer init done");

    traffic_.UpdateLayout(dpi_);
    Trace("traffic layout done");

    // Caption button needs the current squircle width in physical pixels
    // (HWND minus 2*margin on each axis). Recompute on every resize / DPI
    // change too; for the very first layout we read the freshly created
    // window's client rect.
    {
        RECT rc{};
        ::GetClientRect(hwnd, &rc);
        const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
        const int sqW = std::max(1, static_cast<int>(rc.right - rc.left) - 2 * marginPx);
        caption_button_.UpdateLayout(sqW, dpi_);
    }
    Trace("caption-button layout done");

    RelayoutCaptionMenu();
    Trace("caption-menu layout done");

    RelayoutAppAlert();
    Trace("app-alert layout done");

    RelayoutSettings();
    Trace("settings layout done");

    // Toggle the menu when the caption button is clicked. The on_click
    // handler runs synchronously inside CaptionButton::OnLButtonUp.
    caption_button_.SetOnClick([this]() { ToggleMenu(); });

    // The alert button defaults to dismissing the dialog.
    app_alert_.SetOnDismiss([this]() { DismissAlert(); });

    // The Settings sheet's Done button defaults to closing the sheet.
    // The window also reacts to the boolean returned from OnLButtonUp,
    // so this is belt-and-braces for callers that subscribe later.
    settings_.SetOnDone([this]() { HideSettings(); });

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);
    Trace("window shown");

    return hwnd;
}

// ---------------------------------------------------------------------------

void BorderlessWindow::SetSession(terminal::TerminalSession* s) {
    session_ = s;
    renderer_.SetSession(s);

    if (s && hwnd_) {
        const HWND hwndCopy = hwnd_;
        s->SetScheduleRepaint([hwndCopy]() {
            ::PostMessageW(hwndCopy, WM_APP_PTY_DIRTY, 0, 0);
        });
        SyncPtyToSize();
    }
}

// ---------------------------------------------------------------------------

void BorderlessWindow::SyncPtyToSize() {
    if (!session_) return;
    int cols = 0, rows = 0;
    renderer_.GridForCurrentSize(cols, rows);
    if (cols == last_cols_ && rows == last_rows_) return;
    last_cols_ = cols;
    last_rows_ = rows;
    session_->Resize(cols, rows);
}

// ---------------------------------------------------------------------------

void BorderlessWindow::OnDpiChanged(UINT newDpi, const RECT* suggested) {
    dpi_ = newDpi;
    if (suggested) {
        ::SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                       suggested->right - suggested->left,
                       suggested->bottom - suggested->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
    renderer_.SetDpi(dpi_);
    traffic_.UpdateLayout(dpi_);
    {
        RECT rc{};
        ::GetClientRect(hwnd_, &rc);
        const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
        const int sqW = std::max(1, static_cast<int>(rc.right - rc.left) - 2 * marginPx);
        caption_button_.UpdateLayout(sqW, dpi_);
    }
    RelayoutCaptionMenu();
    RelayoutAppAlert();
    RelayoutSettings();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

void BorderlessWindow::RelayoutSettings() {
    if (!hwnd_) return;
    RECT rc{};
    ::GetClientRect(hwnd_, &rc);
    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
    const float swW = std::max(1.0f,
        static_cast<float>(rc.right - rc.left) - 2.0f * marginPx);
    const float swH = std::max(1.0f,
        static_cast<float>(rc.bottom - rc.top) - 2.0f * marginPx);
    const D2D1_RECT_F squircleRect{0.0f, 0.0f, swW, swH};
    const float captionPx = theme::ToPx(theme::kCaptionHeight, dpi_);
    settings_.UpdateLayout(squircleRect, captionPx, dpi_);
}

void BorderlessWindow::ShowSettings() {
    settings_.Show();
    RelayoutSettings();
    StartSettingsAnimation(1.0f);
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void BorderlessWindow::HideSettings() {
    settings_.RequestClose();
    StartSettingsAnimation(0.0f);
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void BorderlessWindow::StartSettingsAnimation(float target) {
    settings_anim_from_   = settings_anim_t_;
    settings_anim_target_ = target;
    settings_anim_start_  = ::GetTickCount();
    if (settings_timer_id_ == 0) {
        settings_timer_id_ = ::SetTimer(hwnd_, kSettingsTimerId,
                                        kSettingsTimerPeriod, nullptr);
    }
}

void BorderlessWindow::OnSettingsTimer() {
    const DWORD now = ::GetTickCount();
    const DWORD dt  = now - settings_anim_start_;
    const float total = static_cast<float>(theme::kSettingsAnimDurationMs);
    const float u = std::clamp(static_cast<float>(dt) / total, 0.0f, 1.0f);

    settings_anim_t_ = settings_anim_from_
                     + (settings_anim_target_ - settings_anim_from_) * u;
    settings_.SetProgress(settings_anim_t_);

    if (u >= 1.0f) {
        ::KillTimer(hwnd_, settings_timer_id_);
        settings_timer_id_ = 0;
        if (settings_anim_target_ <= 0.0f) {
            settings_.SetOpen(false);
        }
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

void BorderlessWindow::RelayoutAppAlert() {
    if (!hwnd_) return;
    RECT rc{};
    ::GetClientRect(hwnd_, &rc);
    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
    const float swW = std::max(1.0f,
        static_cast<float>(rc.right - rc.left) - 2.0f * marginPx);
    const float swH = std::max(1.0f,
        static_cast<float>(rc.bottom - rc.top) - 2.0f * marginPx);
    const D2D1_RECT_F squircleRect{0.0f, 0.0f, swW, swH};
    const float captionPx = theme::ToPx(theme::kCaptionHeight, dpi_);
    app_alert_.UpdateLayout(squircleRect, captionPx, dpi_);
}

void BorderlessWindow::ShowAlert(std::wstring title,
                                 std::wstring message,
                                 std::wstring buttonText) {
    app_alert_.Show(std::move(title), std::move(message), std::move(buttonText));
    RelayoutAppAlert();
    StartAlertAnimation(1.0f);
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void BorderlessWindow::DismissAlert() {
    app_alert_.RequestClose();
    StartAlertAnimation(0.0f);
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void BorderlessWindow::StartAlertAnimation(float target) {
    alert_anim_from_   = alert_anim_t_;
    alert_anim_target_ = target;
    alert_anim_start_  = ::GetTickCount();
    if (alert_timer_id_ == 0) {
        alert_timer_id_ = ::SetTimer(hwnd_, kAlertTimerId,
                                     kAlertTimerPeriod, nullptr);
    }
}

void BorderlessWindow::OnAlertTimer() {
    const DWORD now = ::GetTickCount();
    const DWORD dt  = now - alert_anim_start_;
    const float total = static_cast<float>(theme::kAlertAnimDurationMs);
    const float u = std::clamp(static_cast<float>(dt) / total, 0.0f, 1.0f);

    alert_anim_t_ = alert_anim_from_
                  + (alert_anim_target_ - alert_anim_from_) * u;
    app_alert_.SetProgress(alert_anim_t_);

    if (u >= 1.0f) {
        ::KillTimer(hwnd_, alert_timer_id_);
        alert_timer_id_ = 0;
        if (alert_anim_target_ <= 0.0f) {
            app_alert_.SetOpen(false);
        }
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

void BorderlessWindow::RelayoutCaptionMenu() {
    if (!hwnd_) return;
    RECT rc{};
    ::GetClientRect(hwnd_, &rc);
    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
    const int sqW = std::max(1, static_cast<int>(rc.right - rc.left) - 2 * marginPx);
    caption_menu_.UpdateLayout(caption_button_.Bounds(), sqW, dpi_);
}

void BorderlessWindow::ToggleMenu() {
    StartMenuAnimation(caption_menu_.IsOpen() ? 0.0f : 1.0f);
    if (caption_menu_.IsOpen()) {
        // Closing: keep IsOpen=true through the fade-out; the timer
        // flips it to false at the end. This matches how Apple keeps
        // the panel hit-testable until it's fully gone.
    } else {
        // Opening: relayout *now* so the panel is anchored to the
        // current button bounds (resizes between toggles are rare but
        // possible).
        RelayoutCaptionMenu();
        caption_menu_.SetOpen(true);
    }
}

void BorderlessWindow::StartMenuAnimation(float target) {
    menu_anim_from_   = menu_anim_t_;
    menu_anim_target_ = target;
    menu_anim_start_  = ::GetTickCount();
    if (menu_timer_id_ == 0) {
        menu_timer_id_ = ::SetTimer(hwnd_, kMenuTimerId,
                                    kMenuTimerPeriod, nullptr);
    }
}

void BorderlessWindow::OnMenuTimer() {
    const DWORD now = ::GetTickCount();
    const DWORD dt  = now - menu_anim_start_;
    const float total = static_cast<float>(theme::kCaptionMenuAnimDurationMs);
    const float u = std::clamp(static_cast<float>(dt) / total, 0.0f, 1.0f);
    // Cubic ease-out is applied inside the menu's Render(), so feed the
    // raw progress here and let the menu interpret it.
    const float dir = (menu_anim_target_ >= menu_anim_from_) ? 1.0f : -1.0f;
    if (dir > 0.0f) {
        menu_anim_t_ = menu_anim_from_ + (menu_anim_target_ - menu_anim_from_) * u;
    } else {
        menu_anim_t_ = menu_anim_from_ + (menu_anim_target_ - menu_anim_from_) * u;
    }
    caption_menu_.SetProgress(menu_anim_t_);
    caption_button_.SetExpansion(menu_anim_t_);

    if (u >= 1.0f) {
        ::KillTimer(hwnd_, menu_timer_id_);
        menu_timer_id_ = 0;
        // If we were closing, mark the menu closed only after the fade-out.
        if (menu_anim_target_ <= 0.0f) {
            caption_menu_.SetOpen(false);
        }
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

bool BorderlessWindow::IsInsideContent(int wx, int wy) const {
    const int captionH = theme::ToPxInt(theme::kCaptionHeight, dpi_);
    if (wy < captionH) return false;

    // Squircle width is the HWND minus 2 * shadow margin.
    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
    RECT rc{};
    ::GetWindowRect(hwnd_, &rc);
    const int sqW = (rc.right - rc.left) - 2 * marginPx;
    const int sqH = (rc.bottom - rc.top) - 2 * marginPx;
    return wx >= 0 && wx < sqW && wy >= 0 && wy < sqH;
}

bool BorderlessWindow::ContentPointToCell(int wx, int wy,
                                          int& viewRow, int& col) const {
    if (!session_) return false;

    const int captionH = theme::ToPxInt(theme::kCaptionHeight, dpi_);

    int cols = 0, rows = 0;
    renderer_.GridForCurrentSize(cols, rows);
    if (cols <= 0 || rows <= 0) return false;

    const float padXpx = renderer_.TerminalPaddingXPx();
    const float padYpx = renderer_.TerminalPaddingYPx();
    const float cellW  = renderer_.CellWidthPx();
    const float cellH  = renderer_.CellHeightPx();
    if (cellW < 1.0f || cellH < 1.0f) return false;

    const float gx = static_cast<float>(wx) - padXpx;
    const float gy = static_cast<float>(wy) - captionH - padYpx;

    int c = static_cast<int>(std::floor(gx / cellW));
    int r = static_cast<int>(std::floor(gy / cellH));

    // Selection must be allowed to drag past the right and bottom edges
    // (xterm convention). Clamp into the grid for the lookup but allow
    // the right edge to address column == cols (i.e. one past the last
    // visible cell), so dragging past the rightmost column selects the
    // entire row.
    c = std::clamp(c, 0, cols);
    r = std::clamp(r, 0, rows - 1);
    viewRow = r;
    col     = c;
    return true;
}

// ---------------------------------------------------------------------------

bool BorderlessWindow::CopySelectionToClipboard() {
    if (!session_) return false;
    auto& buf = session_->Buffer();

    std::string utf8;
    {
        std::lock_guard<std::mutex> lk(buf.Lock());
        utf8 = buf.SelectionText();
    }
    if (utf8.empty()) return false;

    // Convert UTF-8 -> UTF-16 for CF_UNICODETEXT, and translate '\n' to
    // "\r\n" so pasting into native editors/Notepad works as expected.
    std::wstring wide;
    {
        const int needed = ::MultiByteToWideChar(
            CP_UTF8, 0, utf8.data(),
            static_cast<int>(utf8.size()), nullptr, 0);
        if (needed <= 0) return false;
        wide.resize(needed);
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                              static_cast<int>(utf8.size()),
                              wide.data(), needed);
    }
    std::wstring crlf;
    crlf.reserve(wide.size() + 16);
    for (wchar_t ch : wide) {
        if (ch == L'\n') crlf.push_back(L'\r');
        crlf.push_back(ch);
    }

    if (!::OpenClipboard(hwnd_)) return false;
    ::EmptyClipboard();

    const SIZE_T bytes = (crlf.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!h) {
        ::CloseClipboard();
        return false;
    }
    if (auto* dst = static_cast<wchar_t*>(::GlobalLock(h))) {
        std::memcpy(dst, crlf.c_str(), bytes);
        ::GlobalUnlock(h);
    }
    ::SetClipboardData(CF_UNICODETEXT, h);
    ::CloseClipboard();
    return true;
}

void BorderlessWindow::PasteFromClipboard() {
    if (!session_) return;
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
    if (!::OpenClipboard(hwnd_)) return;

    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        ::CloseClipboard();
        return;
    }
    auto* wide = static_cast<const wchar_t*>(::GlobalLock(h));
    if (!wide) {
        ::CloseClipboard();
        return;
    }

    // Strip the CR from CRLF; a standalone CR ('\r') is what shells expect
    // for "Enter".
    std::wstring norm;
    norm.reserve(wcslen(wide));
    for (size_t i = 0; wide[i] != L'\0'; ++i) {
        if (wide[i] == L'\r' && wide[i + 1] == L'\n') continue;
        if (wide[i] == L'\n') {
            norm.push_back(L'\r');
        } else {
            norm.push_back(wide[i]);
        }
    }
    ::GlobalUnlock(h);
    ::CloseClipboard();

    // UTF-16 -> UTF-8.
    if (norm.empty()) return;
    const int n = ::WideCharToMultiByte(
        CP_UTF8, 0, norm.data(), static_cast<int>(norm.size()),
        nullptr, 0, nullptr, nullptr);
    if (n <= 0) return;
    std::string utf8(n, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, norm.data(),
                          static_cast<int>(norm.size()),
                          utf8.data(), n, nullptr, nullptr);
    session_->SendInput(utf8.data(), utf8.size());

    // The user's input always brings them back to the live bottom.
    auto& buf = session_->Buffer();
    {
        std::lock_guard<std::mutex> lk(buf.Lock());
        buf.SnapToBottom();
        buf.ClearSelection();
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

LRESULT BorderlessWindow::HitTest(POINT pt) const {
    RECT rc{};
    ::GetWindowRect(hwnd_, &rc);

    const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);

    const RECT sq{
        rc.left   + marginPx,
        rc.top    + marginPx,
        rc.right  - marginPx,
        rc.bottom - marginPx,
    };
    if (pt.x < sq.left || pt.x >= sq.right ||
        pt.y < sq.top  || pt.y >= sq.bottom) {
        return HTTRANSPARENT;
    }

    const int border     = theme::ToPxInt(theme::kResizeBorder,  dpi_);
    const int captionHpx = theme::ToPxInt(theme::kCaptionHeight, dpi_);

    const bool top    = pt.y <  sq.top    + border;
    const bool bottom = pt.y >= sq.bottom - border;
    const bool left   = pt.x <  sq.left   + border;
    const bool right  = pt.x >= sq.right  - border;

    if (top    && left)  return HTTOPLEFT;
    if (top    && right) return HTTOPRIGHT;
    if (bottom && left)  return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (top)    return HTTOP;
    if (bottom) return HTBOTTOM;
    if (left)   return HTLEFT;
    if (right)  return HTRIGHT;

    const int wx = pt.x - sq.left;
    const int wy = pt.y - sq.top;

    if (traffic_.HitTest(wx, wy) != ui::TrafficAction::None) {
        return HTCLIENT;
    }
    if (caption_button_.HitTest(wx, wy)) {
        // Inside the pill - not draggable, not resize. The window proc
        // will receive plain WM_LBUTTON* against this hit so the button
        // handles its own click.
        return HTCLIENT;
    }
    if (caption_menu_.IsOpen()) {
        // While the menu is open, *every* click in the squircle goes
        // through WM_LBUTTONDOWN: clicks on the panel pick items, clicks
        // anywhere else dismiss the menu. We don't want any of those to
        // trigger a window drag or resize.
        return HTCLIENT;
    }
    if (settings_.IsOpen()) {
        // Settings is full-content modal: every click below the caption
        // strip is its own. Caption strip stays draggable.
        if (wy >= captionHpx) return HTCLIENT;
    }
    if (app_alert_.IsOpen()) {
        // Alert is modal: the scrim catches every click below the
        // caption strip. Keep the strip itself draggable (the user can
        // still move the window with a dialog up, like macOS sheets).
        if (wy >= captionHpx) return HTCLIENT;
    }
    if (wy < captionHpx) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

LRESULT BorderlessWindow::WndProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp == TRUE) return 0;
            break;

        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            return HitTest(pt);
        }

        case WM_NCLBUTTONDOWN: {
            const WPARAM ht = wp;
            switch (ht) {
                case HTLEFT: case HTRIGHT: case HTTOP: case HTTOPLEFT:
                case HTTOPRIGHT: case HTBOTTOM: case HTBOTTOMLEFT:
                case HTBOTTOMRIGHT: {
                    const WPARAM dir = SC_SIZE + (ht - HTLEFT + 1);
                    ::SendMessageW(hwnd_, WM_SYSCOMMAND, dir, lp);
                    return 0;
                }
                case HTCAPTION:
                    ::SendMessageW(hwnd_, WM_SYSCOMMAND, SC_MOVE | 0x0002, lp);
                    return 0;
                default:
                    break;
            }
            break;
        }

        // SetCursor: when the mouse is over content, swap to IBEAM. Apple
        // and every other terminal show the text-insert cursor in the
        // grid area. For traffic lights / caption / borders, fall back
        // to DefWindowProc which uses the class cursor (IDC_ARROW).
        case WM_SETCURSOR: {
            const WORD ht = LOWORD(lp);
            if (ht == HTCLIENT) {
                POINT pt{};
                ::GetCursorPos(&pt);
                ::ScreenToClient(hwnd_, &pt);
                const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
                const int wx = pt.x - marginPx;
                const int wy = pt.y - marginPx;
                if (IsInsideContent(wx, wy) &&
                    !settings_.IsOpen() &&
                    !app_alert_.IsOpen() &&
                    traffic_.HitTest(wx, wy) == ui::TrafficAction::None &&
                    !caption_button_.HitTest(wx, wy)) {
                    ::SetCursor(::LoadCursorW(nullptr, IDC_IBEAM));
                    return TRUE;
                }
            }
            break;
        }

        case WM_ACTIVATE:
            active_ = (LOWORD(wp) != WA_INACTIVE);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_SIZE: {
            renderer_.Resize(LOWORD(lp), HIWORD(lp));
            // Caption button is right-anchored, so it has to be re-laid
            // out whenever the squircle width changes.
            const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
            const int sqW = std::max(1, static_cast<int>(LOWORD(lp)) - 2 * marginPx);
            caption_button_.UpdateLayout(sqW, dpi_);
            RelayoutCaptionMenu();
            RelayoutAppAlert();
            RelayoutSettings();
            SyncPtyToSize();
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_DPICHANGED: {
            const UINT newDpi = HIWORD(wp);
            OnDpiChanged(newDpi, reinterpret_cast<const RECT*>(lp));
            SyncPtyToSize();
            return 0;
        }

        // ---- Keyboard input -----------------------------------------------
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (!session_) break;

            const bool ctrl  = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (::GetKeyState(VK_SHIFT)   & 0x8000) != 0;

            // Esc dismisses the caption menu without sending Esc to the
            // shell; only when the menu is actually showing, otherwise
            // the shell is still in charge of Esc.
            if (wp == VK_ESCAPE && app_alert_.IsOpen() && !app_alert_.IsClosing()) {
                DismissAlert();
                return 0;
            }
            if (wp == VK_ESCAPE && settings_.IsOpen() && !settings_.IsClosing()) {
                HideSettings();
                return 0;
            }
            if (wp == VK_ESCAPE && caption_menu_.IsOpen()) {
                StartMenuAnimation(0.0f);
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }

            // Enter / Space commits the alert (acts as the primary
            // button click). Mirrors the keyboard contract of every
            // native macOS alert and Win32 MessageBox.
            if (app_alert_.IsOpen() && !app_alert_.IsClosing() &&
                (wp == VK_RETURN || wp == VK_SPACE)) {
                DismissAlert();
                return 0;
            }

            // While an alert is on screen the shell is frozen out: don't
            // forward keystrokes to the pty until the dialog is gone.
            if (app_alert_.IsOpen()) {
                return 0;
            }

            // Same modality contract for the Settings sheet: while it's
            // visible the terminal can't see keystrokes.
            if (settings_.IsOpen()) {
                return 0;
            }

            // Shortcut: Ctrl+Shift+C = copy. We only intercept it if there
            // is something selected; otherwise let the shell receive its
            // own Ctrl+C (interrupt) via the normal path.
            if (ctrl && shift && wp == 'C') {
                if (CopySelectionToClipboard()) {
                    auto& buf = session_->Buffer();
                    std::lock_guard<std::mutex> lk(buf.Lock());
                    // Apple keeps the selection visible after Cmd-C; we do
                    // the same so the user can re-copy. They can dismiss
                    // it with a click anywhere.
                    return 0;
                }
                // Fall through to normal Ctrl+C.
            }

            // Shortcut: Ctrl+Shift+V = paste.
            if (ctrl && shift && wp == 'V') {
                PasteFromClipboard();
                return 0;
            }

            // Shift+PgUp / Shift+PgDn / Shift+Home / Shift+End: navigate
            // the scrollback. Without Shift these go to the shell as VT
            // sequences (handled by TranslateVirtualKey below).
            if (shift && (wp == VK_PRIOR || wp == VK_NEXT ||
                          wp == VK_HOME  || wp == VK_END)) {
                auto& buf = session_->Buffer();
                std::lock_guard<std::mutex> lk(buf.Lock());
                bool changed = false;
                const int rows = std::max(1, buf.Rows());
                if (wp == VK_PRIOR) changed = buf.ScrollViewport( rows - 1);
                if (wp == VK_NEXT)  changed = buf.ScrollViewport(-(rows - 1));
                if (wp == VK_HOME)  changed = buf.ScrollViewport( buf.MaxViewportOffset());
                if (wp == VK_END)   { buf.SnapToBottom(); changed = true; }
                if (changed) ::InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }

            const uint8_t mods = terminal::CurrentModifiers();
            char buf[16];
            const size_t n = terminal::TranslateVirtualKey(wp, mods, buf,
                                                            sizeof(buf));
            if (n > 0) {
                // Typing always brings the user back to the live bottom and
                // dismisses any active selection (matches Windows Terminal,
                // iTerm2, and other modern terminals).
                auto& tbuf = session_->Buffer();
                {
                    std::lock_guard<std::mutex> lk(tbuf.Lock());
                    tbuf.SnapToBottom();
                    tbuf.ClearSelection();
                }
                session_->SendInput(buf, n);
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            break;
        }

        case WM_CHAR:
        case WM_SYSCHAR: {
            if (!session_) break;
            if (app_alert_.IsOpen()) return 0;
            if (settings_.IsOpen()) return 0;
            char buf[8];
            const size_t n = terminal::TranslateChar(static_cast<wchar_t>(wp),
                                                     buf, sizeof(buf));
            if (n > 0) {
                auto& tbuf = session_->Buffer();
                {
                    std::lock_guard<std::mutex> lk(tbuf.Lock());
                    tbuf.SnapToBottom();
                    tbuf.ClearSelection();
                }
                session_->SendInput(buf, n);
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        case WM_APP_PTY_DIRTY:
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case WM_TIMER:
            if (wp == kMenuTimerId) {
                OnMenuTimer();
                return 0;
            }
            if (wp == kAlertTimerId) {
                OnAlertTimer();
                return 0;
            }
            if (wp == kSettingsTimerId) {
                OnSettingsTimer();
                return 0;
            }
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        // ---- Mouse: selection + scroll ------------------------------------
        case WM_MOUSEMOVE: {
            const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
            const int wx = GET_X_LPARAM(lp) - marginPx;
            const int wy = GET_Y_LPARAM(lp) - marginPx;
            traffic_.OnMouseMove(wx, wy);
            caption_button_.OnMouseMove(wx, wy);
            caption_menu_.OnMouseMove(wx, wy);
            app_alert_.OnMouseMove(wx, wy);
            settings_.OnMouseMove(wx, wy);

            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
            ::TrackMouseEvent(&tme);

            if (selecting_ && session_ && !app_alert_.IsOpen()
                && !settings_.IsOpen()) {
                int viewRow = 0, col = 0;
                if (ContentPointToCell(wx, wy, viewRow, col)) {
                    auto& buf = session_->Buffer();
                    std::lock_guard<std::mutex> lk(buf.Lock());
                    const int64_t absRow = buf.ViewportRowToAbs(viewRow);
                    buf.UpdateSelection(terminal::SelPoint{absRow, col});
                }
            }

            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
        case WM_MOUSELEAVE:
            traffic_.OnMouseLeave();
            caption_button_.OnMouseLeave();
            caption_menu_.OnMouseLeave();
            app_alert_.OnMouseLeave();
            settings_.OnMouseLeave();
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_LBUTTONDOWN: {
            const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
            const int wx = GET_X_LPARAM(lp) - marginPx;
            const int wy = GET_Y_LPARAM(lp) - marginPx;
            ::SetCapture(hwnd_);

            // Modal app alert: clicks inside the panel route to the
            // alert; clicks on the scrim stay swallowed (no dismiss on
            // outside-click, mirroring native macOS modal alerts).
            if (app_alert_.IsOpen() && !app_alert_.IsClosing()) {
                if (app_alert_.HitTestPanel(wx, wy)) {
                    app_alert_.OnLButtonDown(wx, wy);
                }
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            }

            // Modal Settings sheet: clicks inside the sidebar/content
            // panes go to the view; clicks on the scrim stay swallowed.
            if (settings_.IsOpen() && !settings_.IsClosing()) {
                if (settings_.HitTestPanel(wx, wy)) {
                    settings_.OnLButtonDown(wx, wy);
                }
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            }

            // Caption menu is modal-ish: if it's showing, all click-down
            // routes through it first. A click on the menu rows arms the
            // pressed-state; a click outside both the menu and the button
            // dismisses the menu without triggering anything else (Apple
            // does the same on light-dismiss).
            if (caption_menu_.IsOpen()) {
                if (caption_menu_.HitTest(wx, wy)) {
                    caption_menu_.OnLButtonDown(wx, wy);
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                    break;
                }
                if (!caption_button_.HitTest(wx, wy)) {
                    // Click landed outside both the menu and its anchor
                    // button -> animate the menu shut. Don't fall through
                    // to terminal selection; the click "spent" itself on
                    // the dismiss gesture.
                    StartMenuAnimation(0.0f);
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                    break;
                }
            }

            if (traffic_.HitTest(wx, wy) != ui::TrafficAction::None) {
                traffic_.OnLButtonDown(wx, wy);
            } else if (caption_button_.HitTest(wx, wy)) {
                caption_button_.OnLButtonDown(wx, wy);
            } else if (IsInsideContent(wx, wy) && session_) {
                // Begin a fresh selection at this point.
                int viewRow = 0, col = 0;
                if (ContentPointToCell(wx, wy, viewRow, col)) {
                    auto& buf = session_->Buffer();
                    std::lock_guard<std::mutex> lk(buf.Lock());
                    const int64_t absRow = buf.ViewportRowToAbs(viewRow);
                    buf.StartSelection(terminal::SelPoint{absRow, col});
                    selecting_ = true;
                }
            }
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_LBUTTONUP: {
            ::ReleaseCapture();
            const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
            const int wx = GET_X_LPARAM(lp) - marginPx;
            const int wy = GET_Y_LPARAM(lp) - marginPx;

            if (app_alert_.IsOpen() && !app_alert_.IsClosing()) {
                // Alert in front: any click here is its own. The button
                // fires its OnDismiss when the user releases on it.
                app_alert_.OnLButtonUp(wx, wy);
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            }

            if (settings_.IsOpen() && !settings_.IsClosing()) {
                // Settings sheet in front. The view returns true if the
                // user clicked the close-X; otherwise the click selects
                // a row.
                if (settings_.OnLButtonUp(wx, wy)) {
                    HideSettings();
                }
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            }

            if (caption_menu_.IsOpen() && caption_menu_.HitTest(wx, wy)) {
                // Pick fired -> the item handler runs synchronously and
                // typically wants the menu to disappear afterward.
                if (caption_menu_.OnLButtonUp(wx, wy)) {
                    StartMenuAnimation(0.0f);
                }
                ::InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            }

            if (selecting_ && session_) {
                selecting_ = false;
                auto& buf = session_->Buffer();
                std::lock_guard<std::mutex> lk(buf.Lock());
                if (buf.HasSelection()) {
                    if (buf.SelectionText().empty()) {
                        buf.ClearSelection();
                    }
                }
            } else if (caption_button_.HitTest(wx, wy)) {
                caption_button_.OnLButtonUp(wx, wy);
            } else {
                caption_button_.OnLButtonUp(wx, wy);

                const auto fired = traffic_.OnLButtonUp(wx, wy);
                switch (fired) {
                    case ui::TrafficAction::Close:
                        ::PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                        break;
                    case ui::TrafficAction::Minimize:
                        ::ShowWindow(hwnd_, SW_MINIMIZE);
                        break;
                    case ui::TrafficAction::Maximize: {
                        const bool maxed = ::IsZoomed(hwnd_);
                        ::ShowWindow(hwnd_, maxed ? SW_RESTORE : SW_MAXIMIZE);
                        break;
                    }
                    case ui::TrafficAction::None:
                        break;
                }
            }
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        // Right-click: paste (xterm convention). Easier than fishing for
        // the middle button, which laptops rarely have.
        case WM_RBUTTONUP: {
            if (app_alert_.IsOpen()) return 0;
            if (settings_.IsOpen())  return 0;
            PasteFromClipboard();
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (!session_) break;
            if (app_alert_.IsOpen()) return 0;
            if (settings_.IsOpen())  return 0;
            const int delta = GET_WHEEL_DELTA_WPARAM(wp);
            // Lines per wheel notch from the OS. Returns WHEEL_PAGESCROLL
            // for "Scroll one page".
            UINT linesPerNotch = kWheelLinesPerDetent;
            ::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &linesPerNotch, 0);
            if (linesPerNotch == WHEEL_PAGESCROLL) {
                linesPerNotch = static_cast<UINT>(
                    std::max(1, session_->Buffer().Rows() - 1));
            }
            const int notches = delta / WHEEL_DELTA;
            const int lines   = notches * static_cast<int>(linesPerNotch);

            auto& buf = session_->Buffer();
            std::lock_guard<std::mutex> lk(buf.Lock());
            // Wheel up (positive delta) means "back in time" -> increase
            // the viewport offset. Wheel down -> decrease.
            if (buf.ScrollViewport(lines)) {
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            const int marginPx = theme::ToPxInt(theme::kShadowMargin, dpi_);
            mmi->ptMinTrackSize.x = theme::ToPxInt(360.0f, dpi_) + 2 * marginPx;
            mmi->ptMinTrackSize.y = theme::ToPxInt(220.0f, dpi_) + 2 * marginPx;
            return 0;
        }

        case WM_PAINT:
            renderer_.Render(active_, traffic_, caption_button_,
                             caption_menu_, app_alert_, settings_);
            ::ValidateRect(hwnd_, nullptr);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace mactw::window
