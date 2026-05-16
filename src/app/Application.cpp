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
    // Populate the Settings sheet sections. Each item is a category
    // in the sidebar with one or more "cards" (Apple grouped table
    // sections) of rows in the content pane.
    //
    // The handlers wired here are intentionally minimal: toggles flip
    // their internal `toggle_state` automatically inside the view; the
    // closures only run when we want to apply the change to the live
    // profile / theme. Picker rows are wired as `Plain` rows with a
    // chevron and a no-op `on_pick` for now (we'll route them to a
    // sub-page when the navigation layer lands).
    {
        using Row     = ui::SettingsView::Row;
        using Section = ui::SettingsView::Section;
        using Item    = ui::SettingsView::Item;
        using RowKind = ui::SettingsView::RowKind;

        auto& s = window_.GetSettings();

        // Localised primary-button label.
        s.SetDoneLabel(ru ? L"Готово" : L"Done");

        // Apple's tile palette for category icons.
        const theme::Color kTileGray   = theme::Color::FromARGB(0xFF8E8E93);
        const theme::Color kTilePurple = theme::Color::FromARGB(0xFFAF52DE);
        const theme::Color kTileBlue   = theme::Color::FromARGB(0xFF0A84FF);
        const theme::Color kTileGreen  = theme::Color::FromARGB(0xFF34C759);
        const theme::Color kTileTeal   = theme::Color::FromARGB(0xFF40C8E0);

        // ---- General ---------------------------------------------------
        {
            Item it{};
            it.glyph      = L"\u2699";   // gear
            it.label      = ru ? L"Общие"  : L"General";
            it.title      = ru ? L"Общие"  : L"General";
            it.tile_color = kTileGray;

            // Card 1: launch behaviour.
            Section launch{};
            launch.header = ru ? L"ПРИ ЗАПУСКЕ" : L"AT LAUNCH";
            launch.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Запускать с системой" : L"Launch at login",
                L"", false, false, true, nullptr, nullptr});
            launch.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Восстанавливать вкладки" : L"Restore tabs on launch",
                L"", true,  false, true, nullptr, nullptr});
            launch.rows.push_back(Row{RowKind::Value,
                ru ? L"Оболочка по умолчанию" : L"Default shell",
                ru ? L"Авто (PowerShell)"     : L"Auto (PowerShell)",
                false, true, true, nullptr, nullptr});
            it.sections.push_back(std::move(launch));

            // Card 2: language & updates.
            Section lang{};
            lang.header = ru ? L"ЯЗЫК И ОБНОВЛЕНИЯ" : L"LANGUAGE AND UPDATES";
            lang.rows.push_back(Row{RowKind::Value,
                ru ? L"Язык интерфейса" : L"Interface language",
                ru ? L"Системный (Русский)" : L"System (English)",
                false, true, true, nullptr, nullptr});
            lang.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Проверять обновления" : L"Check for updates",
                L"", true, false, true, nullptr, nullptr});
            lang.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Бета-обновления" : L"Beta updates",
                L"", false, false, true, nullptr, nullptr});
            lang.footer = ru
                ? L"Бета-сборки могут содержать недоработанные функции и "
                  L"обновляются чаще основной ветки."
                : L"Beta builds may include unfinished features and ship more "
                  L"often than the stable channel.";
            it.sections.push_back(std::move(lang));

            s.AddItem(std::move(it));
        }

        // ---- Appearance ------------------------------------------------
        {
            Item it{};
            it.glyph      = L"\u25D0";   // half-shaded circle
            it.label      = ru ? L"Внешний вид" : L"Appearance";
            it.title      = ru ? L"Внешний вид" : L"Appearance";
            it.tile_color = kTilePurple;

            Section theme{};
            theme.header = ru ? L"ТЕМА" : L"THEME";
            theme.rows.push_back(Row{RowKind::Value,
                ru ? L"Тема"   : L"Theme",
                ru ? L"Tahoe Dark" : L"Tahoe Dark",
                false, true, true, nullptr, nullptr});
            theme.rows.push_back(Row{RowKind::Value,
                ru ? L"Системный акцент" : L"Accent colour",
                ru ? L"Синий"            : L"Blue",
                false, true, true, nullptr, nullptr});
            theme.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Следовать системной теме"
                   : L"Follow system appearance",
                L"", true, false, true, nullptr, nullptr});
            it.sections.push_back(std::move(theme));

            Section win{};
            win.header = ru ? L"ОКНО" : L"WINDOW";
            win.rows.push_back(Row{RowKind::Value,
                ru ? L"Радиус скруглений" : L"Corner radius",
                L"16 pt", false, true, true, nullptr, nullptr});
            win.rows.push_back(Row{RowKind::Value,
                ru ? L"Непрозрачность" : L"Opacity",
                L"100%",  false, true, true, nullptr, nullptr});
            win.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Тень под окном" : L"Window shadow",
                L"", true, false, true, nullptr, nullptr});
            win.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Прозрачное стекло (Liquid Glass)"
                   : L"Liquid Glass background",
                L"", false, false, true, nullptr, nullptr});
            it.sections.push_back(std::move(win));

            s.AddItem(std::move(it));
        }

        // ---- Prompt ----------------------------------------------------
        {
            Item it{};
            it.glyph      = L"\u276F";   // chevron-right
            it.label      = ru ? L"Промпт" : L"Prompt";
            it.title      = ru ? L"Промпт" : L"Prompt";
            it.tile_color = kTileBlue;

            Section style{};
            style.header = ru ? L"СТИЛЬ" : L"STYLE";
            style.rows.push_back(Row{RowKind::Value,
                ru ? L"Шаблон"           : L"Preset",
                ru ? L"MacTerm (стандарт)" : L"MacTerm (default)",
                false, true, true, nullptr, nullptr});
            style.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Двухстрочный промпт" : L"Two-line prompt",
                L"", true,  false, true, nullptr, nullptr});
            style.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Иконки сегментов" : L"Segment icons",
                L"", true,  false, true, nullptr, nullptr});
            style.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Powerline-разделители"
                   : L"Powerline separators",
                L"", false, false, true, nullptr, nullptr});
            it.sections.push_back(std::move(style));

            Section seg{};
            seg.header = ru ? L"СЕГМЕНТЫ" : L"SEGMENTS";
            seg.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Текущий каталог" : L"Current directory",
                L"", true, false, true, nullptr, nullptr});
            seg.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Git-статус" : L"Git status",
                L"", true, false, true, nullptr, nullptr});
            seg.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Время выполнения" : L"Command duration",
                L"", false, false, true, nullptr, nullptr});
            seg.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Окружение (Python / Node)" : L"Environment (Python / Node)",
                L"", false, false, true, nullptr, nullptr});
            seg.footer = ru
                ? L"Сегменты складываются слева направо. Их порядок и стиль "
                  L"можно будет менять в редакторе промпта."
                : L"Segments stack left to right. The visual editor will let "
                  L"you reorder and restyle them.";
            it.sections.push_back(std::move(seg));

            s.AddItem(std::move(it));
        }

        // ---- Terminal --------------------------------------------------
        {
            Item it{};
            it.glyph      = L"\u25A2";   // square with rounded corners
            it.label      = ru ? L"Терминал" : L"Terminal";
            it.title      = ru ? L"Терминал" : L"Terminal";
            it.tile_color = kTileGreen;

            Section font{};
            font.header = ru ? L"ШРИФТ" : L"FONT";
            font.rows.push_back(Row{RowKind::Value,
                ru ? L"Семейство" : L"Family",
                L"Cascadia Code",  false, true, true, nullptr, nullptr});
            font.rows.push_back(Row{RowKind::Value,
                ru ? L"Размер"    : L"Size",
                L"13 pt",          false, true, true, nullptr, nullptr});
            font.rows.push_back(Row{RowKind::Value,
                ru ? L"Высота строки" : L"Line height",
                L"1.20",           false, true, true, nullptr, nullptr});
            font.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Лигатуры" : L"Ligatures",
                L"", true, false, true, nullptr, nullptr});
            it.sections.push_back(std::move(font));

            Section grid{};
            grid.header = ru ? L"СЕТКА И ПРОКРУТКА" : L"GRID AND SCROLLBACK";
            grid.rows.push_back(Row{RowKind::Value,
                ru ? L"Отступы"    : L"Padding",
                L"12 / 8 pt",      false, true, true, nullptr, nullptr});
            grid.rows.push_back(Row{RowKind::Value,
                ru ? L"Скроллбэк"  : L"Scrollback",
                ru ? L"10 000 строк" : L"10,000 lines",
                false, true, true, nullptr, nullptr});
            grid.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Мерцание курсора" : L"Cursor blink",
                L"", true,  false, true, nullptr, nullptr});
            grid.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Звуковой сигнал (Bell)" : L"Audible bell",
                L"", false, false, true, nullptr, nullptr});
            grid.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Визуальный сигнал" : L"Visual bell",
                L"", true,  false, true, nullptr, nullptr});
            it.sections.push_back(std::move(grid));

            Section pal{};
            pal.header = ru ? L"ПАЛИТРА" : L"PALETTE";
            pal.rows.push_back(Row{RowKind::Value,
                ru ? L"ANSI-палитра" : L"ANSI palette",
                ru ? L"MacTerm Pro"  : L"MacTerm Pro",
                false, true, true, nullptr, nullptr});
            pal.rows.push_back(Row{RowKind::Toggle,
                ru ? L"Жирный = яркий"
                   : L"Bold uses bright colours",
                L"", true, false, true, nullptr, nullptr});
            pal.footer = ru
                ? L"Изменения палитры применяются к новым строкам. Уже "
                  L"отрисованный вывод останется в исходных цветах до "
                  L"следующего перерисовывания."
                : L"Palette changes apply to newly drawn cells. Already-"
                  L"rendered output keeps its original colours until the "
                  L"next repaint.";
            it.sections.push_back(std::move(pal));

            s.AddItem(std::move(it));
        }

        // ---- About -----------------------------------------------------
        {
            Item it{};
            it.glyph      = L"\u2139";   // info
            it.label      = ru ? L"О программе" : L"About";
            it.title      = ru ? L"О программе" : L"About";
            it.tile_color = kTileTeal;

            Section app{};
            app.header = ru ? L"ПРИЛОЖЕНИЕ" : L"APPLICATION";
            app.rows.push_back(Row{RowKind::Value,
                ru ? L"Имя"     : L"Name",
                L"MacTermWin", false, false, true, nullptr, nullptr});
            app.rows.push_back(Row{RowKind::Value,
                ru ? L"Версия"  : L"Version",
                L"0.1.0 (alpha)", false, false, true, nullptr, nullptr});
            app.rows.push_back(Row{RowKind::Value,
                ru ? L"Сборка"  : L"Build",
                L"DComp + Direct2D", false, false, true, nullptr, nullptr});
            it.sections.push_back(std::move(app));

            Section links{};
            links.rows.push_back(Row{RowKind::Plain,
                ru ? L"Сайт проекта" : L"Project website",
                L"", false, true, true, nullptr, nullptr});
            links.rows.push_back(Row{RowKind::Plain,
                ru ? L"Исходный код" : L"Source code",
                L"", false, true, true, nullptr, nullptr});
            links.rows.push_back(Row{RowKind::Plain,
                ru ? L"Сообщить о проблеме" : L"Report an issue",
                L"", false, true, true, nullptr, nullptr});
            links.rows.push_back(Row{RowKind::Plain,
                ru ? L"Лицензии" : L"Acknowledgements",
                L"", false, true, true, nullptr, nullptr});
            it.sections.push_back(std::move(links));

            Section legal{};
            legal.footer = ru
                ? L"MacTermWin — экспериментальный терминал в стиле macOS 26 "
                  L"Tahoe для Windows. Apple, macOS и Tahoe — товарные знаки "
                  L"Apple Inc. и используются здесь только для описания "
                  L"визуального стиля."
                : L"MacTermWin is an experimental macOS 26 Tahoe-styled "
                  L"terminal for Windows. Apple, macOS and Tahoe are "
                  L"trademarks of Apple Inc., used here only to describe "
                  L"the visual style.";
            it.sections.push_back(std::move(legal));

            s.AddItem(std::move(it));
        }
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
