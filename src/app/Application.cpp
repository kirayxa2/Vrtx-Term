#include "app/Application.h"

#include <cstdio>

namespace mactw::app {

namespace {

void Trace(const char* msg) {
    wchar_t tempDir[MAX_PATH] = {};
    if (!::GetTempPathW(MAX_PATH, tempDir)) return;
    wchar_t path[MAX_PATH] = {};
    std::swprintf(path, MAX_PATH, L"%smactermwin.log", tempDir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        std::fprintf(f, "  app: %s\n", msg);
        std::fclose(f);
    }
}

}  // namespace

int Application::Run(HINSTANCE hInstance) {
    Trace("Run() entered");

    // PerMonitorV2 is set in app.manifest, but call SetProcessDpiAwarenessContext
    // as a belt-and-braces measure on systems where the manifest is ignored.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Trace("DPI awareness set");

    window_.Create(hInstance, L"MacTermWin");
    Trace("window created");

    // Spawn the shell once the window has its first valid size. The window
    // pushes the live (cols, rows) into the session via SetSession's
    // initial SyncPtyToSize, so we don't need to know the geometry here -
    // we just have to pick *some* sane starting grid for Start() before
    // the first WM_SIZE arrives.
    if (!session_.Start(80, 24)) {
        Trace("session start FAILED");
        ::MessageBoxW(nullptr,
                      L"Failed to launch a shell. PowerShell, pwsh, "
                      L"powershell.exe and cmd.exe were all unreachable.",
                      L"MacTermWin", MB_OK | MB_ICONERROR);
    } else {
        Trace("session started");
    }
    window_.SetSession(&session_);
    Trace("session bound to window");

    // Populate the caption-menu with default items. Labels are localised
    // by the user's UI language: Russian if the primary language is RU,
    // English otherwise. The handlers run synchronously on the UI thread
    // when the user picks an item, so they're free to touch HWNDs.
    {
        const LANGID langId = ::GetUserDefaultUILanguage();
        const bool   ru     = (PRIMARYLANGID(langId) == LANG_RUSSIAN);

        auto& menu = window_.GetCaptionMenu();

        // Settings: not yet implemented; show a quick tooltip-equivalent
        // via MessageBox so the user can confirm the wiring works.
        menu.AddItem({
            L"\u2699",   // U+2699 GEAR
            ru ? L"Настройки" : L"Settings",
            [hwnd = window_.Hwnd(), ru]() {
                ::MessageBoxW(hwnd,
                    ru ? L"Окно настроек ещё не готово."
                       : L"Settings UI is not implemented yet.",
                    ru ? L"Настройки" : L"Settings",
                    MB_OK | MB_ICONINFORMATION);
            }
        });

        // Reset terminal: clears the screen via VT escape (CSI 2J + CUP
        // 1;1 + reset SGR) and the scrollback (CSI 3J). Doesn't kill the
        // shell - the prompt re-appears after the next pty heartbeat.
        menu.AddItem({
            L"\u21BA",   // U+21BA ANTICLOCKWISE OPEN CIRCLE ARROW
            ru ? L"Сбросить терминал" : L"Reset terminal",
            [this]() {
                const char kReset[] = "\x1b[H\x1b[2J\x1b[3J\x1b[0m";
                session_.SendInput(kReset, sizeof(kReset) - 1);
            }
        });

        // About: short blurb about the project.
        menu.AddItem({
            L"\u2139",   // U+2139 INFORMATION SOURCE
            ru ? L"О программе" : L"About",
            [hwnd = window_.Hwnd(), ru]() {
                ::MessageBoxW(hwnd,
                    ru ? L"MacTermWin\nТерминал в стиле macOS Tahoe для Windows."
                       : L"MacTermWin\nA macOS Tahoe-styled terminal for Windows.",
                    ru ? L"О программе" : L"About",
                    MB_OK | MB_ICONINFORMATION);
            }
        });
    }
    Trace("caption-menu items configured");

    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    Trace("message loop exited");

    // Tear the session down before the window so the reader thread doesn't
    // race with the dying renderer.
    session_.Stop();
    return static_cast<int>(msg.wParam);
}

}  // namespace mactw::app
