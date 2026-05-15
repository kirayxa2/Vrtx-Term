// TerminalSession - the glue between ConPty (OS pipes + child shell),
// VtParser (escape-sequence interpreter), and TermBuffer (the grid model).
//
// Lifecycle:
//   ctor                 -> build TermBuffer + VtParser, wire WriteBack.
//   Start(cols, rows)    -> spawn shell via ConPty, hook OnPtyOutput to
//                            parser (under buf.Lock()).
//   Resize(cols, rows)   -> resize TermBuffer + ResizePseudoConsole.
//   SendInput(bytes)     -> forward keystrokes to the pty.
//   ScheduleRepaint(fn)  -> caller registers a "wake the UI thread" hook;
//                            session calls it whenever the grid is dirty.
//                            Typical impl: PostMessage(hwnd, WM_APP+1, 0, 0)
//                            then WM_PAINT.
//   Stop() / dtor        -> tear everything down in the right order.
//
// Threading model:
//   * UI thread: SendInput, Resize, Buffer().Lock() during WM_PAINT.
//   * ConPty reader thread: calls into Session::OnPtyBytes which takes
//     buf.Lock(), feeds the parser, then nudges the UI via repaint_fn_.
//
// Nothing here is templated or virtual; it's a 100-line plumbing class.

#pragma once

#include "pch.h"
#include "terminal/ConPty.h"
#include "terminal/TermBuffer.h"
#include "terminal/VtParser.h"

namespace mactw::terminal {

using ScheduleRepaintFn = std::function<void()>;

class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();

    TerminalSession(const TerminalSession&)            = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    // Spawn the shell at the requested grid size. Returns false on failure.
    bool Start(int cols, int rows);

    // Update grid size; ResizePseudoConsole notifies the shell.
    void Resize(int cols, int rows);

    // Send raw bytes to the shell stdin (already in the encoding the shell
    // expects - UTF-8 + VT escapes for special keys).
    void SendInput(const char* data, size_t len);

    // Hook the UI repaint scheduler. Optional; no-op if unset.
    void SetScheduleRepaint(ScheduleRepaintFn fn) { repaint_ = std::move(fn); }

    // Buffer access for the renderer.
    TermBuffer&       Buffer()       { return buffer_; }
    const TermBuffer& Buffer() const { return buffer_; }

    bool Running() const { return pty_.Running(); }

    void Stop();

private:
    void OnPtyBytes(const char* data, size_t len);

    TermBuffer        buffer_;
    VtParser          parser_;
    ConPty            pty_;
    ScheduleRepaintFn repaint_;
};

}  // namespace mactw::terminal
