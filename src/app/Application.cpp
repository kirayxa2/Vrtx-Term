#include "app/Application.h"

namespace mactw::app {

int Application::Run(HINSTANCE hInstance) {
    // PerMonitorV2 is set in app.manifest, but call SetProcessDpiAwareness as
    // a belt-and-braces measure on systems where the manifest is ignored
    // (e.g. when launched from older shells).
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    window_.Create(hInstance, L"MacTermWin");

    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

}  // namespace mactw::app
