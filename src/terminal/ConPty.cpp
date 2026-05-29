#include "terminal/ConPty.h"

#include "terminal/Profile.h"

#include <cstdio>

namespace vrtx::terminal {

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

// True when `path` ends in pwsh.exe or powershell.exe (case-insensitive).
// Used to decide whether to inject our default profile.
bool IsPowerShell(const std::wstring& path) {
    auto lower = [](std::wstring s) {
        for (auto& c : s) c = static_cast<wchar_t>(::towlower(c));
        return s;
    };
    const std::wstring lp = lower(path);
    return lp.size() >= 8 &&
           (lp.rfind(L"pwsh.exe")       == lp.size() - 8 ||
            lp.rfind(L"powershell.exe") == lp.size() - 14);
}

// Quote a path for inclusion on a Windows command line. We always emit
// double-quotes; embedded quotes are escaped per CommandLineToArgvW
// rules (back-slash escaping). Good enough for filesystem paths.
std::wstring QuoteArg(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 2);
    out.push_back(L'"');
    for (wchar_t c : s) {
        if (c == L'"') out.push_back(L'\\');
        out.push_back(c);
    }
    out.push_back(L'"');
    return out;
}

// Directory portion of a path (everything before the last separator).
std::wstring DirOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

// Case-insensitive test for an "KEY=" prefix on an environment entry.
bool EnvEntryHasKey(const wchar_t* entry, const wchar_t* key) {
    size_t i = 0;
    for (; key[i]; ++i) {
        if (!entry[i] || ::towlower(entry[i]) != ::towlower(key[i])) return false;
    }
    return entry[i] == L'=';
}

// Build a CREATE_UNICODE_ENVIRONMENT block: the parent environment plus the
// variables MSYS2 needs to behave like a proper interactive shell.
//
//   MSYSTEM=MSYS         selects the base POSIX environment - this is what
//                        puts /usr/bin on PATH (so pacman and the rest of
//                        the core tools resolve) and drives the prompt set
//                        up by /etc/profile.
//   MSYS2_PATH_TYPE=inherit  keeps the Windows PATH visible inside bash, so
//                        users can still call git.exe / where.exe / etc.
//
// Any pre-existing copies of those keys are dropped so ours win.
std::vector<wchar_t> BuildMsys2Env() {
    std::vector<wchar_t> out;
    auto appendVar = [&out](const wchar_t* kv) {
        for (const wchar_t* p = kv; *p; ++p) out.push_back(*p);
        out.push_back(L'\0');
    };

    static const wchar_t* const kOurKeys[] = {
        L"MSYSTEM", L"CHERE_INVOKING", L"MSYS2_PATH_TYPE",
    };

    if (LPWCH env = ::GetEnvironmentStringsW()) {
        for (LPWCH p = env; *p;) {
            const size_t len = ::wcslen(p);
            bool drop = false;
            for (const wchar_t* key : kOurKeys) {
                if (EnvEntryHasKey(p, key)) { drop = true; break; }
            }
            if (!drop) {
                out.insert(out.end(), p, p + len);
                out.push_back(L'\0');
            }
            p += len + 1;
        }
        ::FreeEnvironmentStringsW(env);
    }

    appendVar(L"MSYSTEM=MSYS");
    appendVar(L"MSYS2_PATH_TYPE=inherit");

    out.push_back(L'\0');  // double-null terminator for the block.
    return out;
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

std::wstring ConPty::ResolveCmd() {
    if (auto sysroot = EnvVar(L"SystemRoot"); !sysroot.empty()) {
        std::wstring candidate = sysroot + L"\\System32\\cmd.exe";
        if (FileExists(candidate)) return candidate;
    }
    return L"cmd.exe";
}

std::wstring ConPty::ResolveMsys2Bash() {
    // A real MSYS2 install keeps bash.exe and pacman.exe side by side in
    // <root>\usr\bin. We require BOTH so we don't accidentally launch
    // Git-Bash or WSL's bash (neither of which has pacman) when the user
    // asked specifically for the MSYS2 experience.
    auto tryRoot = [](const std::wstring& root) -> std::wstring {
        if (root.empty()) return L"";
        const std::wstring bash   = root + L"\\usr\\bin\\bash.exe";
        const std::wstring pacman = root + L"\\usr\\bin\\pacman.exe";
        if (FileExists(bash) && FileExists(pacman)) return bash;
        return L"";
    };

    // 1) Explicit override - lets power users point us at any install.
    if (auto r = tryRoot(EnvVar(L"MSYS2_ROOT")); !r.empty()) return r;

    // 2) Conventional install roots (\msys64 / \msys32) on EVERY drive
    //    present in the system, not just C:. MSYS2 is portable and is very
    //    often installed on a secondary drive (D:, E:, ...). We probe the
    //    system drive first so a typical install is found immediately.
    {
        std::vector<std::wstring> drives;
        if (auto sys = EnvVar(L"SystemDrive"); !sys.empty()) drives.push_back(sys);
        const DWORD mask = ::GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (!(mask & (1u << i))) continue;
            std::wstring d(1, static_cast<wchar_t>(L'A' + i));
            d += L':';
            if (std::find(drives.begin(), drives.end(), d) == drives.end())
                drives.push_back(d);
        }
        for (const auto& drive : drives) {
            for (const wchar_t* name : {L"\\msys64", L"\\msys32"}) {
                if (auto r = tryRoot(drive + name); !r.empty()) return r;
            }
        }
    }

    // 3) Scoop user install.
    if (auto up = EnvVar(L"USERPROFILE"); !up.empty()) {
        if (auto r = tryRoot(up + L"\\scoop\\apps\\msys2\\current"); !r.empty())
            return r;
    }

    // 4) Last resort: bash.exe on PATH, accepted only if pacman.exe lives
    //    next to it (i.e. it really is an MSYS2 bash).
    if (auto p = SearchOnPath(L"bash.exe"); !p.empty()) {
        const std::wstring dir = DirOf(p);
        if (!dir.empty() && FileExists(dir + L"\\pacman.exe")) return p;
    }
    return L"";
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

bool ConPty::Start(int cols, int rows, ShellKind kind,
                   const std::wstring& cmdline) {
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

    // 2) Resolve shell + build command line (and, for MSYS2, a custom
    //    environment block so the prompt and pacman work).
    //
    // A non-empty `cmdline` overrides everything and is launched verbatim.
    // Otherwise we branch on `kind`.
    std::wstring fullCmd;
    std::vector<wchar_t> envBlock;  // empty => inherit parent (nullptr).
    DWORD creationFlags = EXTENDED_STARTUPINFO_PRESENT;

    if (!cmdline.empty()) {
        shell_path_ = cmdline;
        fullCmd     = cmdline;
    } else if (kind == ShellKind::Msys2Bash) {
        shell_path_ = ResolveMsys2Bash();
        if (shell_path_.empty()) {
            // No MSYS2 install found - let the caller surface a hint.
            CleanupHandles();
            return false;
        }
        // --login sources /etc/profile (puts /usr/bin on PATH so pacman and
        // friends resolve, and sets up the MSYS2 environment); -i makes the
        // shell interactive so ~/.bashrc with the coloured PS1 prompt runs.
        fullCmd       = QuoteArg(shell_path_) + L" --login -i";
        envBlock      = BuildMsys2Env();
        creationFlags |= CREATE_UNICODE_ENVIRONMENT;
    } else if (kind == ShellKind::Cmd) {
        shell_path_ = ResolveCmd();
        fullCmd     = shell_path_;
    } else {
        // Auto / PowerShell. Resolve the PowerShell family and inject our
        // own minimal profile (rounded prompt, no oh-my-posh, no winfetch).
        // The user's $PROFILE is suppressed via -NoProfile so we get a
        // deterministic look out of the box. If Auto falls all the way
        // through to cmd.exe it is launched as-is.
        shell_path_ = ResolveShell();
        if (IsPowerShell(shell_path_)) {
            const std::wstring profile = EnsureDefaultPwshProfile();
            fullCmd = QuoteArg(shell_path_);
            fullCmd += L" -NoLogo -NoProfile -NoExit";
            if (!profile.empty()) {
                // -File runs the script and stays interactive thanks to
                // -NoExit; we cannot use -File on Windows PowerShell 5.1
                // together with -NoExit reliably, so we use `. <path>` via
                // -Command which works on both pwsh 7+ and powershell 5.1.
                fullCmd += L" -Command \". '";
                fullCmd += profile;
                fullCmd += L"'\"";
            }
        } else {
            fullCmd = shell_path_;
        }
    }
    std::wstring mutableCmd = fullCmd;  // CreateProcessW may modify the buffer.

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
        creationFlags,
        envBlock.empty() ? nullptr : envBlock.data(),  // lpEnvironment
        nullptr,                        // lpCurrentDirectory
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

}  // namespace vrtx::terminal
