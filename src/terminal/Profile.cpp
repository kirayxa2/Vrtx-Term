#include "terminal/Profile.h"

#include <cstdio>

namespace mactw::terminal {

namespace {

std::wstring EnvVar(const wchar_t* name) {
    wchar_t buf[1024] = {};
    DWORD n = ::GetEnvironmentVariableW(name, buf, 1024);
    if (n == 0 || n >= 1024) return L"";
    return std::wstring(buf, n);
}

// PowerShell prompt template. UTF-8 source, written verbatim to disk.
//
// Layout:
//
//     ╭─user@host  /current/dir
//     ╰─$
//
// (or `#` instead of `$` when the session is elevated). Colours come from
// VT escapes so they survive any PSReadLine / colourless terminal:
//
//     CYAN  for the rounded corners + dashes
//     GREEN for "user@host"
//     BLUE  for the path
//     RED   for the "#" when admin, default for "$"
//
// The shell knows the user via $env:USERNAME and the host via [System.Net.
// Dns]::GetHostName() (faster than $env:COMPUTERNAME on domain machines).
// Admin detection uses the WindowsPrincipal API which is built-in and
// doesn't require any module load.
constexpr const char* kProfileTemplate =
    "# MacTermWin default PowerShell profile - regenerated on every launch.\n"
    "# Do not hand-edit; place customisations in $PROFILE instead and we'll\n"
    "# look at exposing a 'load user profile too' option later.\n"
    "\n"
    "$Host.UI.RawUI.WindowTitle = 'MacTermWin'\n"
    "\n"
    "# Detect once at session start; admin status doesn't change mid-session.\n"
    "$script:MacTw_IsAdmin = $false\n"
    "try {\n"
    "    $id = [System.Security.Principal.WindowsIdentity]::GetCurrent()\n"
    "    $pr = New-Object System.Security.Principal.WindowsPrincipal($id)\n"
    "    $script:MacTw_IsAdmin = $pr.IsInRole(\n"
    "        [System.Security.Principal.WindowsBuiltInRole]::Administrator)\n"
    "} catch { }\n"
    "\n"
    "$script:MacTw_Host = try { [System.Net.Dns]::GetHostName() }\n"
    "                    catch { $env:COMPUTERNAME }\n"
    "\n"
    "function global:prompt {\n"
    "    $e    = [char]27\n"
    "    $cyan = \"$e[38;2;108;168;226m\"  # ANSI 4 from our palette\n"
    "    $grn  = \"$e[38;2;91;194;115m\"   # ANSI 2\n"
    "    $blu  = \"$e[38;2;137;194;255m\"  # ANSI 12 (bright blue, more readable on dark)\n"
    "    $red  = \"$e[38;2;255;123;133m\"  # ANSI 9\n"
    "    $rst  = \"$e[0m\"\n"
    "\n"
    "    $u = $env:USERNAME\n"
    "    $h = $script:MacTw_Host\n"
    "    $p = (Get-Location).Path\n"
    "    # Replace $HOME with ~ for a tighter prompt; fall back to literal\n"
    "    # path if $HOME isn't set (rare, but happens in service contexts).\n"
    "    if ($env:HOME) { $p = $p -replace [regex]::Escape($env:HOME), '~' }\n"
    "    elseif ($env:USERPROFILE) { $p = $p -replace [regex]::Escape($env:USERPROFILE), '~' }\n"
    "\n"
    "    $sigil = if ($script:MacTw_IsAdmin) { \"$red#$rst\" } else { '$' }\n"
    "\n"
    "    \"$cyan\u256d\u2500$rst$grn$u@$h$rst  $blu$p$rst`n$cyan\u2570\u2500$rst$sigil \"\n"
    "}\n"
    "\n"
    "# A friendly one-line greeting on session start - no winfetch, no\n"
    "# oh-my-posh, just enough to know which shell launched.\n"
    "$psv = $PSVersionTable.PSVersion\n"
    "Write-Host (\"MacTermWin -> PowerShell $($psv.Major).$($psv.Minor)\") `\n"
    "    -ForegroundColor DarkGray\n";

bool EnsureDirectoryExists(const std::wstring& dir) {
    DWORD attrs = ::GetFileAttributesW(dir.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return ::CreateDirectoryW(dir.c_str(), nullptr) ||
           ::GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteUtf8(const std::wstring& path, const char* data, size_t len) {
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    // PowerShell happily reads UTF-8 without a BOM, but adding one makes
    // older Windows PowerShell 5.1 also pick the encoding correctly.
    static const unsigned char kBom[] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    BOOL  ok = ::WriteFile(h, kBom, sizeof(kBom), &written, nullptr) &&
               ::WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
    ::CloseHandle(h);
    return ok != FALSE;
}

}  // namespace

// ---------------------------------------------------------------------------

std::wstring EnsureDefaultPwshProfile() {
    std::wstring base = EnvVar(L"LOCALAPPDATA");
    if (base.empty()) return L"";

    const std::wstring dir = base + L"\\MacTermWin";
    if (!EnsureDirectoryExists(dir)) return L"";

    const std::wstring path = dir + L"\\profile.ps1";

    const size_t len = std::char_traits<char>::length(kProfileTemplate);
    if (!WriteUtf8(path, kProfileTemplate, len)) {
        // Couldn't refresh; if a previous copy exists we still proceed.
        DWORD attrs = ::GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return L"";
    }
    return path;
}

}  // namespace mactw::terminal
