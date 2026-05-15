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
