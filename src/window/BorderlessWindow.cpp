#include "window/BorderlessWindow.h"

#include "theme/TahoeTheme.h"
#include "window/GlassEffect.h"
#include "window/SquircleGeometry.h"

namespace mactw::window {

namespace {
constexpr wchar_t kClassName[] = L"MacTermWin.BorderlessWindow";

// Helper: get the DPI for the monitor a given HWND lives on. Falls back to
// 96 on systems where GetDpiForWindow is unavailable (only really pre-1607).
UINT GetWindowDpiSafe(HWND hwnd) {
    using PFN = UINT(WINAPI*)(HWND);
    static PFN fn = []() -> PFN {
        const HMODULE u = ::GetModuleHandleW(L"user32.dll");
        return u ? reinterpret_cast<PFN>(::GetProcAddress(u, "GetDpiForWindow"))
                 : nullptr;
    }();
    return fn ? fn(hwnd) : 96u;
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

    auto trace = [](const char* msg) {
        wchar_t tempDir[MAX_PATH] = {};
        if (!::GetTempPathW(MAX_PATH, tempDir)) return;
        wchar_t path[MAX_PATH] = {};
        std::swprintf(path, MAX_PATH, L"%smactermwin.log", tempDir);
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"a") == 0 && f) {
            std::fprintf(f, "  win: %s\n", msg);
            std::fclose(f);
        }
    };
    trace("Create() entered");

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &BorderlessWindow::StaticWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // we paint everything
    wc.lpszClassName = kClassName;
    ::RegisterClassExW(&wc);
    trace("class registered");

    // Initial DPI of the primary monitor.
    dpi_ = ::GetDpiForSystem();

    const int wPx = theme::ToPxInt(theme::kDefaultWindowWidth,  dpi_);
    const int hPx = theme::ToPxInt(theme::kDefaultWindowHeight, dpi_);

    constexpr DWORD style   = WS_OVERLAPPEDWINDOW;
    constexpr DWORD exStyle = 0;

    HWND hwnd = ::CreateWindowExW(
        exStyle, kClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, wPx, hPx,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) {
        throw std::runtime_error("CreateWindowExW failed");
    }
    trace("hwnd created");

    dpi_ = GetWindowDpiSafe(hwnd);

    MARGINS m{0, 0, 1, 0};
    ::DwmExtendFrameIntoClientArea(hwnd, &m);
    trace("DWM extended");

    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                       SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    trace("frame changed");

    renderer_.SetDpi(dpi_);
    trace("calling renderer init");
    renderer_.Initialize(hwnd);
    trace("renderer init done");

    traffic_.UpdateLayout(dpi_);
    trace("traffic layout done");

    // Acrylic blur backdrop. Safe to no-op on systems that lack the API.
    ApplyAcrylicBackdrop(hwnd);
    trace("acrylic applied");

    UpdateWindowRegion();
    trace("region applied");

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);
    trace("window shown");

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
    // SetWindowRgn takes ownership of the region; we don't free it.
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

    // Window-relative coords for child element checks.
    const int wx = pt.x - rc.left;
    const int wy = pt.y - rc.top;

    // Traffic-lights area: consume clicks ourselves (HTCLIENT).
    if (traffic_.HitTest(wx, wy) != ui::TrafficAction::None) {
        return HTCLIENT;
    }

    // Inside the caption strip but not on a button: act as drag handle.
    if (wy < captionHpx) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

LRESULT BorderlessWindow::WndProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE: {
            // Returning 0 tells DWM "no non-client frame at all". Our entire
            // window rect is treated as client area. The tiny extended frame
            // from DwmExtendFrameIntoClientArea above still gives us shadow.
            //
            // Caveat: when maximized, DWM positions our window 8px past every
            // monitor edge (so the system frame is offscreen). We compensate
            // by inflating the client rect inward by exactly that amount.
            if (wp == TRUE) {
                if (::IsZoomed(hwnd_)) {
                    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                    const int frameX = ::GetSystemMetricsForDpi(SM_CXFRAME, dpi_) +
                                       ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
                    const int frameY = ::GetSystemMetricsForDpi(SM_CYFRAME, dpi_) +
                                       ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
                    params->rgrc[0].left   += frameX;
                    params->rgrc[0].top    += frameY;
                    params->rgrc[0].right  -= frameX;
                    params->rgrc[0].bottom -= frameY;
                }
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            return HitTest(pt);
        }

        case WM_NCACTIVATE: {
            // Returning TRUE prevents DWM from drawing its activation frame
            // (which we don't have anyway). lParam == -1 means "no repaint".
            active_ = (wp != FALSE);
            ::InvalidateRect(hwnd_, nullptr, FALSE);
            return TRUE;
        }

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
            // Track WM_MOUSELEAVE so the hover state clears when the cursor
            // leaves the client area.
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
            // Don't let minimum size collapse below useful UI metrics.
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
            // We paint everything ourselves; tell GDI not to touch the bg.
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
