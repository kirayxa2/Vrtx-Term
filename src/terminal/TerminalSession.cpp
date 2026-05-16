#include "terminal/TerminalSession.h"

namespace vrtx::terminal {

TerminalSession::TerminalSession()
    : parser_(buffer_) {
    // Replies emitted by the parser (CPR, DA1) loop straight back into pty
    // stdin. Multiple threads may end up calling Write() concurrently, but
    // ConPty::Write takes a single ::WriteFile call which is atomic at the
    // pipe level.
    parser_.SetWriteBack([this](const char* d, size_t n) {
        pty_.Write(d, n);
    });
}

TerminalSession::~TerminalSession() {
    Stop();
}

bool TerminalSession::Start(int cols, int rows) {
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    buffer_.Resize(cols, rows);

    pty_.SetOnOutput([this](const char* d, size_t n) {
        OnPtyBytes(d, n);
    });
    pty_.SetOnExit([this](uint32_t /*code*/) {
        if (repaint_) repaint_();
    });

    return pty_.Start(cols, rows);
}

void TerminalSession::Resize(int cols, int rows) {
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    {
        std::lock_guard<std::mutex> lk(buffer_.Lock());
        buffer_.Resize(cols, rows);
    }
    pty_.Resize(cols, rows);
}

void TerminalSession::SendInput(const char* data, size_t len) {
    pty_.Write(data, len);
}

void TerminalSession::OnPtyBytes(const char* data, size_t len) {
    {
        std::lock_guard<std::mutex> lk(buffer_.Lock());
        parser_.Feed(data, len);
    }
    if (repaint_) repaint_();
}

void TerminalSession::Stop() {
    pty_.Stop();
}

}  // namespace vrtx::terminal
