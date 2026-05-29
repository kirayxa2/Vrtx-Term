// Top-level application object. Owns the main window and runs the message
// pump. Kept tiny on purpose so main() stays readable.

#pragma once

#include "pch.h"
#include "terminal/TerminalSession.h"
#include "window/BorderlessWindow.h"

namespace vrtx::app {

class Application {
public:
    int Run(HINSTANCE hInstance);

private:
    window::BorderlessWindow   window_;

    // Multi-tab session management.
    std::vector<std::unique_ptr<terminal::TerminalSession>> sessions_;
    int active_tab_{0};

    // Which shell the "+" new-tab button (and the first tab) launches.
    // Changed from the Settings sheet's "Default shell" row.
    terminal::ShellKind default_shell_{terminal::ShellKind::Auto};

    void OpenNewTab(terminal::ShellKind kind);
    void CloseTab(int index);
    void SwitchTab(int index);
    void RebuildTabBar();
};

}  // namespace vrtx::app
