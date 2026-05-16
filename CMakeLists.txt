cmake_minimum_required(VERSION 3.20)
project(MacTermWin
    VERSION 0.1.0
    DESCRIPTION "macOS Tahoe-styled terminal for Windows"
    LANGUAGES CXX)

# ---- Global settings -------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR
        "MacTermWin is a Windows-only application. "
        "Configure on Windows with Visual Studio 2022 or Build Tools.")
endif()

if(NOT MSVC)
    message(WARNING
        "This project targets MSVC. MinGW/Clang may work but is untested.")
endif()

# ---- Compiler flags --------------------------------------------------------

if(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /utf-8
        /MP                     # parallel compilation
        /EHsc
    )
    add_compile_definitions(
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00     # Windows 10
        WINVER=0x0A00
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ---- Sources ---------------------------------------------------------------

set(MACTERMWIN_SOURCES
    src/main.cpp

    src/app/Application.cpp

    src/window/BorderlessWindow.cpp
    src/window/SquircleGeometry.cpp
    src/window/GlassEffect.cpp

    src/render/Renderer.cpp
    src/render/TerminalView.cpp
    src/render/BoxDrawing.cpp

    src/terminal/ConPty.cpp
    src/terminal/TermBuffer.cpp
    src/terminal/TermInput.cpp
    src/terminal/TerminalSession.cpp
    src/terminal/VtParser.cpp
    src/terminal/Profile.cpp

    src/ui/TrafficLights.cpp
    src/ui/CaptionButton.cpp
    src/ui/CaptionMenu.cpp
    src/ui/AppAlert.cpp

    src/theme/TahoeTheme.cpp
)

set(MACTERMWIN_HEADERS
    src/pch.h

    src/app/Application.h

    src/window/BorderlessWindow.h
    src/window/SquircleGeometry.h
    src/window/GlassEffect.h

    src/render/Renderer.h
    src/render/TerminalView.h
    src/render/BoxDrawing.h

    src/terminal/ConPty.h
    src/terminal/TermBuffer.h
    src/terminal/TermInput.h
    src/terminal/TerminalSession.h
    src/terminal/VtParser.h
    src/terminal/Profile.h

    src/ui/TrafficLights.h
    src/ui/CaptionButton.h
    src/ui/CaptionMenu.h
    src/ui/AppAlert.h

    src/theme/TahoeTheme.h
)

set(MACTERMWIN_RESOURCES
    resources/app.rc
)

# ---- Target ----------------------------------------------------------------

add_executable(MacTermWin WIN32
    ${MACTERMWIN_SOURCES}
    ${MACTERMWIN_HEADERS}
    ${MACTERMWIN_RESOURCES}
)

target_include_directories(MacTermWin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_precompile_headers(MacTermWin PRIVATE
    src/pch.h
)

target_link_libraries(MacTermWin PRIVATE
    d2d1
    d3d11
    dxgi
    dwrite
    dcomp
    dwmapi
    windowscodecs
    user32
    gdi32
    shcore
    uxtheme
)

# We embed our own manifest via resources/app.rc (it sets PerMonitorV2 DPI
# awareness). Tell the MSVC linker not to also auto-generate one, otherwise
# the resource compiler errors out with CVT1100 ("duplicate resource") on the
# RT_MANIFEST entry.
if(MSVC)
    target_link_options(MacTermWin PRIVATE "/MANIFEST:NO")
endif()

set_target_properties(MacTermWin PROPERTIES
    OUTPUT_NAME "MacTermWin"
    VS_DPI_AWARE "PerMonitor"
)

# Copy artifacts to a stable bin/ folder for convenience
set_target_properties(MacTermWin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
