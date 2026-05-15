#include "app/Application.h"

#include <cstdio>

namespace {

// Append a line to %TEMP%\mactermwin.log. Used as a tracer of last resort
// when the app dies before any UI can show.
void TraceLog(const char* msg) {
    wchar_t tempDir[MAX_PATH] = {};
    if (!::GetTempPathW(MAX_PATH, tempDir)) return;

    wchar_t path[MAX_PATH] = {};
    std::swprintf(path, MAX_PATH, L"%smactermwin.log", tempDir);

    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        std::fprintf(f, "%s\n", msg);
        std::fclose(f);
    }
}

void ShowFatal(const char* what) {
    TraceLog(what);
    // Convert ASCII -> wchar_t. Our error messages are pure ASCII (HRESULT
    // hex strings + short identifiers), so this trivial widening is fine.
    std::wstring wide;
    for (const char* p = what; *p; ++p) {
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
    }
    ::MessageBoxW(nullptr, wide.c_str(),
                  L"MacTermWin fatal error",
                  MB_ICONERROR | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    TraceLog("---- MacTermWin start ----");
    try {
        mactw::app::Application app;
        const int rc = app.Run(hInstance);
        TraceLog("clean exit");
        return rc;
    } catch (const std::exception& e) {
        ShowFatal(e.what());
        return 1;
    } catch (...) {
        ShowFatal("Unknown fatal error (non-std exception)");
        return 1;
    }
}
