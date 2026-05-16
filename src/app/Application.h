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
    terminal::TerminalSession  session_;
};

}  // namespace vrtx::app
