#include "window/BorderlessWindow.h"

#include "theme/TahoeTheme.h"
#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

constexpr wchar_t kClassName[] = L"MacTermWin.BorderlessWindow";

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

    const int wPx = theme::ToPxInt(theme::kDefaultWindowWidth,  dpi_);
    const int hPx = theme::ToPxInt(theme::kDefaultWindowHeight, dpi_);

    // Plain WS_POPUP. We don't need WS_THICKFRAME because we manage resize
    // ourselves via WM_NCHITTEST + WM_SYSCOMMAND/SC_SIZE. WS_MINIMIZEBOX and
    // WS_MAXIMIZEBOX exist solely so the taskbar preview offers those
    // actions.
    constexpr DWORD style   = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                              WS_CLIPCHILDREN;

    // The crucial flag: WS_EX_NOREDIRECTIONBITMAP. Without a redirection
    // surface there is no place for DWM to paint frame chrome. Combined
    // with DirectComposition rendering, every visible pixel is one we put
    // in the DComp visual.
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

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);
    Trace("window shown");

    return hwnd;
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
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------

LRESULT BorderlessWindow::HitTest(POINT pt) const {
    RECT rc{};
    ::GetWindowRect(hwnd_, &rc);

    const int border = theme::ToPxInt(theme::kResizeBorder, dpi_);
    const int captionHpx = theme::ToPxInt(theme::kCaptionHeight, dpi_);

    const bool top    = pt.y < rc.top    + border;
    const bool bottom = pt.y >= rc.bottom - border;
    const bool left   = pt.x < rc.left   + border;
    const bool right  = pt.x >= rc.right  - border;

    if (top    && left)  return HTTOPLEFT;
    if (top    && right) return HTTOPRIGHT;
    if (bottom && left)  return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (top)    return HTTOP;
    if (bottom) return HTBOTTOM;
    if (left)   return HTLEFT;
    if (right)  return HTRIGHT;

    const int wx = pt.x - rc.left;
    const int wy = pt.y - rc.top;

    if (traffic_.HitTest(wx, wy) != ui::TrafficAction::None) {
        return HTCLIENT;
    }
    if (wy < captionHpx) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

LRESULT BorderlessWindow::WndProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        // No non-client area at all: the entire window rect is client.
        case WM_NCCALCSIZE:
            if (wp == TRUE) return 0;
            break;

        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            return HitTest(pt);
        }

        // Without WS_THICKFRAME we drive the resize/move loops manually.
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

        case WM_ACTIVATE:
            active_ = (LOWORD(wp) != WA_INACTIVE);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_SIZE:
            renderer_.Resize(LOWORD(lp), HIWORD(lp));
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_DPICHANGED: {
            const UINT newDpi = HIWORD(wp);
            OnDpiChanged(newDpi, reinterpret_cast<const RECT*>(lp));
            return 0;
        }

        case WM_MOUSEMOVE: {
            traffic_.OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
            ::TrackMouseEvent(&tme);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
        case WM_MOUSELEAVE:
            traffic_.OnMouseLeave();
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_LBUTTONDOWN:
            traffic_.OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            ::SetCapture(hwnd_);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;

        case WM_LBUTTONUP: {
            ::ReleaseCapture();
            const auto fired = traffic_.OnLButtonUp(GET_X_LPARAM(lp),
                                                    GET_Y_LPARAM(lp));
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
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = theme::ToPxInt(360.0f, dpi_);
            mmi->ptMinTrackSize.y = theme::ToPxInt(220.0f, dpi_);
            return 0;
        }

        case WM_PAINT:
            renderer_.Render(active_, traffic_);
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
