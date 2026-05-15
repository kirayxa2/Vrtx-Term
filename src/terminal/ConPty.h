// Thin RAII wrapper around the Windows ConPTY API.
//
// ConPTY (introduced in Windows 10 1809) is the modern way to embed a real
// console session into your own process. Conceptually it is a pair of
// anonymous pipes plus an opaque HPCON handle that the OS uses to pump VT
// sequences in/out of the child shell. The shell itself runs as a separate
// child process with its stdio attached to those pipes.
//
// Pipeline:
//
//     [child shell stdout] -> hPipeOut -> reader thread -> on_output_(bytes)
//     [our keystrokes]     -> hPipeIn  -> child shell stdin
//
//     ResizePseudoConsole(cols, rows)  -> shell sees SIGWINCH equivalent.
//
// We resolve the shell at Start() time:
//     1. pwsh.exe via PATH
//     2. PowerShell 7 default install location
//     3. powershell.exe (built-in PS 5.1)
//     4. cmd.exe (last resort)
//
// All public methods are safe to call from the UI thread. The reader thread
// is internal and joined in Stop()/dtor.

#pragma once

#include "pch.h"

#include <atomic>
#include <functional>
#include <thread>

namespace mactw::terminal {

using OnPtyOutputFn = std::function<void(const char* data, size_t len)>;
using OnPtyExitFn   = std::function<void(uint32_t exitCode)>;

class ConPty {
public:
    ConPty();
    ~ConPty();

    ConPty(const ConPty&)            = delete;
    ConPty& operator=(const ConPty&) = delete;

    // Spawn a shell with an attached pseudo-console of `cols` x `rows`.
    // Returns false on error (and leaves the object in a clean state).
    //
    // `cmdline` may be empty - we then auto-pick a shell. Pass an explicit
    // command line (e.g. L"cmd.exe /k") to override.
    bool Start(int cols, int rows, const std::wstring& cmdline = L"");

    // Idempotent: terminates the child if running, joins the reader thread,
    // and releases all handles.
    void Stop();

    // Send raw bytes to the child stdin. UTF-8 / VT input encoding is the
    // caller's responsibility.
    void Write(const char* data, size_t len);

    // Notify the child of a window-size change. Safe to call from any thread.
    void Resize(int cols, int rows);

    // Callbacks. Set before calling Start().
    void SetOnOutput(OnPtyOutputFn fn) { on_output_ = std::move(fn); }
    void SetOnExit  (OnPtyExitFn   fn) { on_exit_   = std::move(fn); }

    bool Running() const { return running_.load(std::memory_order_acquire); }

    // The shell binary actually launched (resolved at Start()).
    const std::wstring& ShellPath() const { return shell_path_; }

private:
    static std::wstring ResolveShell();
    void   ReaderLoop();
    void   CleanupHandles();

    // Build a STARTUPINFOEX with an EXTENDED_STARTUPINFO_PRESENT thread-attr
    // list that pins the pseudo-console to the new process.
    bool   PrepareStartupInfo(STARTUPINFOEXW& si,
                              std::vector<unsigned char>& attrListStorage);

    HPCON  hpcon_   {nullptr};
    HANDLE pipe_in_ {nullptr};   // we write here -> shell reads stdin
    HANDLE pipe_out_{nullptr};   // shell writes -> we read

    // The "other ends" given to the child. We close our copies as soon as
    // CreatePseudoConsole returns; the child keeps them via the pty.
    HANDLE child_in_  {nullptr};
    HANDLE child_out_ {nullptr};

    HANDLE process_   {nullptr};
    HANDLE thread_h_  {nullptr};
    DWORD  process_id_{0};

    std::thread       reader_;
    std::atomic<bool> running_{false};

    OnPtyOutputFn on_output_;
    OnPtyExitFn   on_exit_;

    std::wstring  shell_path_;
};

}  // namespace mactw::terminal
