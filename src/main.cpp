#include "app/Application.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    try {
        mactw::app::Application app;
        return app.Run(hInstance);
    } catch (const std::exception& e) {
        // Last-resort error UI. Real diagnostics will go through a logging
        // module once the terminal core lands.
        const std::string what = e.what();
        std::wstring wide(what.begin(), what.end());
        ::MessageBoxW(nullptr, wide.c_str(), L"MacTermWin fatal error",
                      MB_ICONERROR | MB_OK);
        return 1;
    } catch (...) {
        ::MessageBoxW(nullptr, L"Unknown fatal error", L"MacTermWin",
                      MB_ICONERROR | MB_OK);
        return 1;
    }
}
