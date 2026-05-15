#include "window/BorderlessWindow.h"

#include "theme/TahoeTheme.h"
#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {

constexpr wchar_t kClassName[] = L"MacTermWin.BorderlessWindow";

// Undocumented messages DWM uses to ask us to paint chunks of the system
// non-client area. We want none of it; default-handling them re-introduces
// the grey/blue caption frame even when WM_NCPAINT itself is suppressed.
constexpr UINT kWmNcUahDrawCaption = 0x00AE;
constexpr UINT kWmNcUahDrawFrame   = 0x00AF;

// Get the per-monitor DPI; falls back to 96 only on pre-1607 systems.
UINT GetWindowDpiSafe(HWND hwnd) {
    using PFN = UINT(WINAPI*)(HWND);
    static PFN fn = []() -> PFN {
        const HMODULE u = ::GetModuleHandleW(L"user32.dll");
        return u ? reinterpret_cast<PFN>(::GetProcAddress(u, "GetDpiForWindow"))
                 : nullptr;
    }();
    return fn ? fn(hwnd) : 96u;
}

// Tell DWM to stop drawing on this window:
//   * NCRENDERING_POLICY = DISABLED disables every system-drawn frame
//     element. Available since Windows Vista, behaviour stable on Win11.
//   * WINDOW_CORNER_PREFERENCE = DONOTROUND defeats the Win11-only
//     auto-rounding that would otherwise clip our squircle region.
void NeutraliseDwm(HWND hwnd) {
    constexpr DWORD DWMWA_NCRENDERING_POLICY_LOCAL          = 2;
    constexpr DWORD DWMNCRP_DISABLED_LOCAL                  = 1;
    constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_LOCAL    = 33;
    constexpr DWORD DWMWCP_DONOTROUND_LOCAL                 = 1;
    constexpr DWORD DWMWA_TRANSITIONS_FORCEDISABLED_LOCAL   = 3;

    DWORD policy = DWMNCRP_DISABLED_LOCAL;
    ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY_LOCAL,
                            &policy, sizeof(policy));

    DWORD cornerPref = DWMWCP_DONOTROUND_LOCAL;
    ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_LOCAL,
                            &cornerPref, sizeof(cornerPref));

    BOOL noTransitions = TRUE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED_LOCAL,
                            &noTransitions, sizeof(noTransitions));
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
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
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
    wc.hbrBackground = nullptr;  // we paint everything
    wc.lpszClassName = kClassName;
    ::RegisterClassExW(&wc);
    Trace("class registered");

    dpi_ = ::GetDpiForSystem();

    const int wPx = theme::ToPxInt(theme::kDefaultWindowWidth,  dpi_);
    const int hPx = theme::ToPxInt(theme::kDefaultWindowHeight, dpi_);

    // Pure popup. No WS_THICKFRAME, so DWM has nothing to paint a frame on.
    // We re-implement resize ourselves: WM_NCHITTEST returns HTLEFT etc.,
    // and on WM_NCLBUTTONDOWN we trigger SC_SIZE manually instead of relying
    // on DefWindowProc's frame-driven path.
    constexpr DWORD style   = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                              WS_CLIPCHILDREN;
    constexpr DWORD exStyle = WS_EX_APPWINDOW;

    HWND hwnd = ::CreateWindowExW(
        exStyle, kClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, wPx, hPx,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) {
        throw std::runtime_error("CreateWindowExW failed");
    }
    Trace("hwnd created");

    dpi_ = GetWindowDpiSafe(hwnd);

    // Kill every DWM-side decoration on this HWND.
    NeutraliseDwm(hwnd);
    Trace("DWM neutralised");

    renderer_.SetDpi(dpi_);
    Trace("calling renderer init");
    renderer_.Initialize(hwnd);
    Trace("renderer init done");

    traffic_.UpdateLayout(dpi_);
    Trace("traffic layout done");

    UpdateWindowRegion();
    Trace("region applied");

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);
    Trace("window shown");

    return hwnd;
}

// ---------------------------------------------------------------------------

void BorderlessWindow::UpdateWindowRegion() {
    if (!hwnd_) return;
    RECT rc{};
    ::GetWindowRect(hwnd_, &rc);
    const int w = rc.right  - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    const int radius = theme::ToPxInt(theme::kRadiusTitlebarWindow, dpi_);
    HRGN rgn = BuildSquircleRegion(w, h, radius, theme::kSquircleSmoothing);
    ::SetWindowRgn(hwnd_, rgn, /*redraw=*/TRUE);
}

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
    UpdateWindowRegion();
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
        // ---- Frame suppression --------------------------------------------
        //
        // NCCALCSIZE returning 0 collapses the entire non-client area into
        // the client rect.
        case WM_NCCALCSIZE: {
            if (wp == TRUE) {
                if (::IsZoomed(hwnd_)) {
                    auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                    const int frameX = ::GetSystemMetricsForDpi(SM_CXFRAME, dpi_) +
                                       ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
                    const int frameY = ::GetSystemMetricsForDpi(SM_CYFRAME, dpi_) +
                                       ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
                    p->rgrc[0].left   += frameX;
                    p->rgrc[0].top    += frameY;
                    p->rgrc[0].right  -= frameX;
                    p->rgrc[0].bottom -= frameY;
                }
                return 0;
            }
            break;
        }

        // Anything that asks us to paint NC is silenced.
        case WM_NCPAINT:                     return 0;
        case WM_NCACTIVATE:                  active_ = (wp != FALSE);
                                             ::InvalidateRect(hwnd_, nullptr, FALSE);
                                             return TRUE;
        case kWmNcUahDrawCaption:            return 0;
        case kWmNcUahDrawFrame:              return 0;

        // ---- Hit testing & resize -----------------------------------------
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            return HitTest(pt);
        }

        // We removed WS_THICKFRAME, so DefWindowProc no longer kicks off a
        // resize loop on its own. Do it ourselves.
        case WM_NCLBUTTONDOWN: {
            const WPARAM ht = wp;
            switch (ht) {
                case HTLEFT: case HTRIGHT: case HTTOP:    case HTTOPLEFT:
                case HTTOPRIGHT: case HTBOTTOM: case HTBOTTOMLEFT:
                case HTBOTTOMRIGHT: {
                    static constexpr WPARAM kEdgeMap[] = {
                        0,           SC_SIZE | 0xF001, SC_SIZE | 0xF002,
                        SC_SIZE | 0xF003, SC_SIZE | 0xF004, SC_SIZE | 0xF005,
                        SC_SIZE | 0xF006, SC_SIZE | 0xF007, SC_SIZE | 0xF008,
                    };
                    // Map HT* (10..17) -> SC_SIZE direction (1..8).
                    const WPARAM dir = SC_SIZE + (ht - HTLEFT + 1);
                    ::SendMessageW(hwnd_, WM_SYSCOMMAND, dir, lp);
                    (void)kEdgeMap;  // kept for documentation
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

        // ---- Activation, sizing, painting --------------------------------
        case WM_ACTIVATE: {
            active_ = (LOWORD(wp) != WA_INACTIVE);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_SIZE: {
            UpdateWindowRegion();
            renderer_.Resize(LOWORD(lp), HIWORD(lp));
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_DPICHANGED: {
            const UINT newDpi = HIWORD(wp);
            OnDpiChanged(newDpi, reinterpret_cast<const RECT*>(lp));
            return 0;
        }

        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            traffic_.OnMouseMove(x, y);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
            ::TrackMouseEvent(&tme);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
        case WM_MOUSELEAVE: {
            traffic_.OnMouseLeave();
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_LBUTTONDOWN: {
            traffic_.OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            ::SetCapture(hwnd_);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }

        case WM_LBUTTONUP: {
            ::ReleaseCapture();
            const auto fired = traffic_.OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
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

        case WM_PAINT: {
            renderer_.Render(active_, traffic_);
            ::ValidateRect(hwnd_, nullptr);
            return 0;
        }

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
