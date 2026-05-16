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
    bool sessionStarted = session_.Start(80, 24);
    if (!sessionStarted) {
        Trace("session start FAILED");
    } else {
        Trace("session started");
    }
    window_.SetSession(&session_);
    Trace("session bound to window");

    const LANGID langId = ::GetUserDefaultUILanguage();
    const bool   ru     = (PRIMARYLANGID(langId) == LANG_RUSSIAN);

    // If the shell wouldn't launch, surface the failure as one of our
    // own alerts (no MessageBoxW). The terminal stays empty until the
    // user dismisses; they can then close the window with the close
    // traffic light.
    if (!sessionStarted) {
        window_.ShowAlert(
            ru ? L"Не удалось запустить оболочку"
               : L"Failed to launch a shell",
            ru ? L"PowerShell, pwsh, powershell.exe и cmd.exe оказались "
                 L"недоступны. Установите PowerShell или проверьте PATH "
                 L"и попробуйте снова."
               : L"PowerShell, pwsh, powershell.exe and cmd.exe were all "
                 L"unreachable. Install PowerShell or check PATH and try again.",
            L"OK");
    }

    // Populate the caption-menu with default items. Labels are localised
    // by the user's UI language: Russian if the primary language is RU,
    // English otherwise. The handlers run synchronously on the UI thread
    // when the user picks an item, so they're free to touch HWNDs.
    {
        auto& menu = window_.GetCaptionMenu();

        // Settings: open the in-window Settings sheet (sidebar + content).
        menu.AddItem({
            L"\u2699",   // U+2699 GEAR
            ru ? L"Настройки" : L"Settings",
            [this]() {
                window_.ShowSettings();
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
            [this, ru]() {
                window_.ShowAlert(
                    ru ? L"О программе" : L"About",
                    ru ? L"MacTermWin\nТерминал в стиле macOS Tahoe для Windows."
                       : L"MacTermWin\nA macOS Tahoe-styled terminal for Windows.",
                    L"OK");
            }
        });
    }
    // Populate the Settings sheet sections. Each row is a category in
    // the sidebar; the active row's title + body are shown in the
    // content pane on the right. Real settings controls (toggles,
    // pickers, sliders) will land on top of this scaffolding.
    {
        auto& s = window_.GetSettings();

        s.AddItem({
            L"\u2699",   // gear
            ru ? L"Общие"      : L"General",
            ru ? L"Общие"      : L"General",
            ru ? L"Базовые параметры приложения. В будущих версиях здесь "
                 L"будут язык интерфейса, поведение при запуске и "
                 L"автообновления."
               : L"Application-wide basics. Future versions will expose "
                 L"interface language, startup behaviour, and auto-update "
                 L"cadence here.",
            nullptr,
        });

        s.AddItem({
            L"\u25CE",   // bullseye - placeholder for "Appearance"
            ru ? L"Внешний вид" : L"Appearance",
            ru ? L"Внешний вид" : L"Appearance",
            ru ? L"Темы (Tahoe Light / Tahoe Dark), радиус скруглений, "
                 L"непрозрачность окна и шрифты chrome будут жить здесь."
               : L"Themes (Tahoe Light / Tahoe Dark), corner radii, window "
                 L"opacity and chrome fonts will live here.",
            nullptr,
        });

        s.AddItem({
            L"\u276F",   // > shell prompt arrow
            ru ? L"Промпт"      : L"Prompt",
            ru ? L"Промпт"      : L"Prompt",
            ru ? L"Конструктор однострочного и многострочного промпта (как у "
                 L"oh-my-posh / starship). Можно будет настроить сегменты, "
                 L"цвета, иконки и Git-статус."
               : L"Builder for single- and multi-line prompts (oh-my-posh / "
                 L"starship style). You will be able to tune segments, "
                 L"colours, icons and Git status display.",
            nullptr,
        });

        s.AddItem({
            L"\u2316",   // crosshair - "Terminal"
            ru ? L"Терминал"    : L"Terminal",
            ru ? L"Терминал"    : L"Terminal",
            ru ? L"Шрифт и его размер, lineHeight, ANSI-палитра, размер "
                 L"скроллбэка и поведение Bell. Сейчас все значения "
                 L"захардкожены в TahoeTheme."
               : L"Font face and size, line height, ANSI palette, scrollback "
                 L"depth and bell behaviour. The values are currently "
                 L"hard-coded in TahoeTheme.",
            nullptr,
        });

        s.AddItem({
            L"\u2139",   // info
            ru ? L"О программе" : L"About",
            ru ? L"О программе" : L"About",
            ru ? L"MacTermWin\nТерминал в стиле macOS 26 Tahoe для Windows.\n"
                 L"Сделано в виде эксперимента над DComp + D2D без DWM-"
                 L"отрисовки окна."
               : L"MacTermWin\nA macOS 26 Tahoe-styled terminal for Windows.\n"
                 L"Built as an experiment with DirectComposition + Direct2D "
                 L"and zero DWM-painted chrome.",
            nullptr,
        });
    }
    Trace("settings sections configured");

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
