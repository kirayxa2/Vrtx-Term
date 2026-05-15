// Top-level application object. Owns the main window and runs the message
// pump. Kept tiny on purpose so main() stays readable.

#pragma once

#include "pch.h"
#include "window/BorderlessWindow.h"

namespace mactw::app {

class Application {
public:
    int Run(HINSTANCE hInstance);

private:
    window::BorderlessWindow window_;
};

}  // namespace mactw::app
