#include "terminal/ConPty.h"

#include <cstdio>

namespace mactw::terminal {

namespace {

bool FileExists(const std::wstring& path) {
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// Look up `name` on PATH. Returns the absolute path or an empty string.
std::wstring SearchOnPath(const wchar_t* name) {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = ::SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH) return L"";
    return std::wstring(buf, n);
}

std::wstring EnvVar(const wchar_t* name) {
    wchar_t buf[1024] = {};
    DWORD n = ::GetEnvironmentVariableW(name, buf, 1024);
    if (n == 0 || n >= 1024) return L"";
    return std::wstring(buf, n);
}

}  // namespace

// ---------------------------------------------------------------------------

ConPty::ConPty() = default;
ConPty::~ConPty() { Stop(); }

// ---- Shell resolution -----------------------------------------------------

std::wstring ConPty::ResolveShell() {
    // 1) pwsh on PATH (PowerShell 7+).
    if (auto p = SearchOnPath(L"pwsh.exe"); !p.empty()) return p;

    // 2) Default PS7 install location.
    if (auto pf = EnvVar(L"ProgramFiles"); !pf.empty()) {
        std::wstring candidate = pf + L"\\PowerShell\\7\\pwsh.exe";
        if (FileExists(candidate)) return candidate;
    }

    // 3) Built-in Windows PowerShell 5.1.
    if (auto p = SearchOnPath(L"powershell.exe"); !p.empty()) return p;

    // 4) cmd.exe via SystemRoot.
    if (auto sysroot = EnvVar(L"SystemRoot"); !sysroot.empty()) {
        std::wstring candidate = sysroot + L"\\System32\\cmd.exe";
        if (FileExists(candidate)) return candidate;
    }
    return L"cmd.exe";
}

// ---- Startup-info plumbing ------------------------------------------------

bool ConPty::PrepareStartupInfo(STARTUPINFOEXW& si,
                                std::vector<unsigned char>& storage) {
    SIZE_T size = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (size == 0) return false;

    storage.assign(size, 0);
    si.lpAttributeList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());

    if (!::InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size)) {
        return false;
    }

    if (!::UpdateProcThreadAttribute(
            si.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            hpcon_,
            sizeof(HPCON),
            nullptr,
            nullptr)) {
        ::DeleteProcThreadAttributeList(si.lpAttributeList);
        return false;
    }

    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    return true;
}

// ---- Start ----------------------------------------------------------------

bool ConPty::Start(int cols, int rows, const std::wstring& cmdline) {
    if (running_.load()) return false;

    // 1) Two pipes:
    //    - pipe_in_  (we write) <--> child_in_  (child reads stdin)
    //    - child_out_ (child writes stdout) <--> pipe_out_ (we read)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = FALSE;

    if (!::CreatePipe(&child_in_, &pipe_in_, &sa, 0)) {
        CleanupHandles();
        return false;
    }
    if (!::CreatePipe(&pipe_out_, &child_out_, &sa, 0)) {
        CleanupHandles();
        return false;
    }

    COORD size{static_cast<SHORT>(std::max(1, cols)),
               static_cast<SHORT>(std::max(1, rows))};

    HRESULT hr = ::CreatePseudoConsole(size, child_in_, child_out_, 0, &hpcon_);
    if (FAILED(hr)) {
        CleanupHandles();
        return false;
    }

    // The child needs its own handles to the pipes; we already gave them to
    // CreatePseudoConsole, so we can release our duplicates.
    ::CloseHandle(child_in_);  child_in_  = nullptr;
    ::CloseHandle(child_out_); child_out_ = nullptr;

    // 2) Resolve shell + build command line.
    shell_path_ = cmdline.empty() ? ResolveShell() : cmdline;
    std::wstring mutableCmd = shell_path_;  // CreateProcessW may modify the buffer.

    // 3) STARTUPINFOEX with the pseudo-console attribute attached.
    STARTUPINFOEXW si{};
    std::vector<unsigned char> attrStorage;
    if (!PrepareStartupInfo(si, attrStorage)) {
        CleanupHandles();
        return false;
    }

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(
        nullptr,
        mutableCmd.data(),
        nullptr, nullptr,
        FALSE,                          // bInheritHandles - explicitly FALSE.
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr,
        &si.StartupInfo,
        &pi);

    ::DeleteProcThreadAttributeList(si.lpAttributeList);

    if (!ok) {
        CleanupHandles();
        return false;
    }

    process_    = pi.hProcess;
    thread_h_   = pi.hThread;
    process_id_ = pi.dwProcessId;

    running_.store(true, std::memory_order_release);
    reader_ = std::thread([this] { ReaderLoop(); });
    return true;
}

// ---- Reader thread --------------------------------------------------------

void ConPty::ReaderLoop() {
    constexpr DWORD kBufSize = 4096;
    char buffer[kBufSize];

    while (running_.load(std::memory_order_acquire)) {
        DWORD bytesRead = 0;
        const BOOL ok =
            ::ReadFile(pipe_out_, buffer, kBufSize, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            // ERROR_BROKEN_PIPE means the child closed its stdout - normal exit.
            break;
        }
        if (on_output_) {
            on_output_(buffer, static_cast<size_t>(bytesRead));
        }
    }

    running_.store(false, std::memory_order_release);

    DWORD exitCode = 0;
    if (process_) {
        ::WaitForSingleObject(process_, 1000);
        ::GetExitCodeProcess(process_, &exitCode);
    }
    if (on_exit_) on_exit_(exitCode);
}

// ---- Public ops -----------------------------------------------------------

void ConPty::Write(const char* data, size_t len) {
    if (!pipe_in_ || !running_.load()) return;
    DWORD written = 0;
    ::WriteFile(pipe_in_, data, static_cast<DWORD>(len), &written, nullptr);
}

void ConPty::Resize(int cols, int rows) {
    if (!hpcon_) return;
    COORD size{static_cast<SHORT>(std::max(1, cols)),
               static_cast<SHORT>(std::max(1, rows))};
    ::ResizePseudoConsole(hpcon_, size);
}

void ConPty::Stop() {
    if (!running_.load() && !process_ && !hpcon_) return;

    running_.store(false, std::memory_order_release);

    // Closing the input pipe usually nudges the shell to quit cleanly.
    if (pipe_in_)  { ::CloseHandle(pipe_in_);  pipe_in_  = nullptr; }

    // Closing the pseudo-console also unblocks the reader's ReadFile.
    if (hpcon_)    { ::ClosePseudoConsole(hpcon_); hpcon_ = nullptr; }

    if (reader_.joinable()) reader_.join();

    if (pipe_out_) { ::CloseHandle(pipe_out_); pipe_out_ = nullptr; }

    if (process_) {
        // If the child is still running 250ms after we closed its pipes,
        // hammer it - leaking it would block app shutdown.
        if (::WaitForSingleObject(process_, 250) == WAIT_TIMEOUT) {
            ::TerminateProcess(process_, 1);
        }
        ::CloseHandle(process_);  process_  = nullptr;
    }
    if (thread_h_) { ::CloseHandle(thread_h_); thread_h_ = nullptr; }

    process_id_ = 0;
}

void ConPty::CleanupHandles() {
    if (hpcon_)     { ::ClosePseudoConsole(hpcon_); hpcon_     = nullptr; }
    if (pipe_in_)   { ::CloseHandle(pipe_in_);   pipe_in_   = nullptr; }
    if (pipe_out_)  { ::CloseHandle(pipe_out_);  pipe_out_  = nullptr; }
    if (child_in_)  { ::CloseHandle(child_in_);  child_in_  = nullptr; }
    if (child_out_) { ::CloseHandle(child_out_); child_out_ = nullptr; }
    if (process_)   { ::CloseHandle(process_);   process_   = nullptr; }
    if (thread_h_)  { ::CloseHandle(thread_h_);  thread_h_  = nullptr; }
}

}  // namespace mactw::terminal
