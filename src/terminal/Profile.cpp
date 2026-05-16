#include "terminal/Profile.h"

#include <cstdio>

namespace vrtx::terminal {

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
    "# VrtxTerm default PowerShell profile - regenerated on every launch.\n"
    "# Do not hand-edit; place customisations in $PROFILE instead and we'll\n"
    "# look at exposing a 'load user profile too' option later.\n"
    "\n"
    "$Host.UI.RawUI.WindowTitle = 'VrtxTerm'\n"
    "\n"
    "# Detect once at session start; admin status doesn't change mid-session.\n"
    "$script:VrtxTerm_IsAdmin = $false\n"
    "try {\n"
    "    $id = [System.Security.Principal.WindowsIdentity]::GetCurrent()\n"
    "    $pr = New-Object System.Security.Principal.WindowsPrincipal($id)\n"
    "    $script:VrtxTerm_IsAdmin = $pr.IsInRole(\n"
    "        [System.Security.Principal.WindowsBuiltInRole]::Administrator)\n"
    "} catch { }\n"
    "\n"
    "$script:VrtxTerm_Host = try { [System.Net.Dns]::GetHostName() }\n"
    "                    catch { $env:COMPUTERNAME }\n"
    "\n"
    "# ---- Welcome banner -----------------------------------\n"
    "# System Terminal prints exactly:\n"
    "#   Last login: Mon Jan  6 23:06:18 on ttys001\n"
    "# day-of-month is right-padded to two columns with a SPACE when single\n"
    "# digit, not a zero. We mirror that format and remember the timestamp\n"
    "# in %LOCALAPPDATA%\\VrtxTerm\\last_login.txt so the next session can\n"
    "# print our own previous login. The very first session has no prior\n"
    "# timestamp so we just skip the banner - same as a fresh mac account.\n"
    "$script:VrtxTerm_LlDir  = Join-Path $env:LOCALAPPDATA 'VrtxTerm'\n"
    "$script:VrtxTerm_LlFile = Join-Path $script:VrtxTerm_LlDir 'last_login.txt'\n"
    "if (-not (Test-Path $script:VrtxTerm_LlDir)) {\n"
    "    New-Item -ItemType Directory -Path $script:VrtxTerm_LlDir -Force | Out-Null\n"
    "}\n"
    "$script:VrtxTerm_LlInv = [System.Globalization.CultureInfo]::InvariantCulture\n"
    "function global:Format-VrtxTermLoginTime([datetime]$dt) {\n"
    "    $dow   = $dt.ToString('ddd', $script:VrtxTerm_LlInv)\n"
    "    $mon   = $dt.ToString('MMM', $script:VrtxTerm_LlInv)\n"
    "    $day   = $dt.Day.ToString().PadLeft(2)\n"
    "    $clock = $dt.ToString('HH:mm:ss', $script:VrtxTerm_LlInv)\n"
    "    \"$dow $mon $day $clock\"\n"
    "}\n"
    "if (Test-Path $script:VrtxTerm_LlFile) {\n"
    "    $prev = (Get-Content -Path $script:VrtxTerm_LlFile -Raw -ErrorAction SilentlyContinue)\n"
    "    if ($prev) {\n"
    "        Write-Host (\"Last login: $($prev.Trim()) on ttys001\")\n"
    "    }\n"
    "}\n"
    "Set-Content -Path $script:VrtxTerm_LlFile `\n"
    "    -Value (Format-VrtxTermLoginTime (Get-Date)) -NoNewline -Encoding utf8\n"
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
    "    $h = $script:VrtxTerm_Host\n"
    "    $p = (Get-Location).Path\n"
    "    # Replace $HOME with ~ for a tighter prompt; fall back to literal\n"
    "    # path if $HOME isn't set (rare, but happens in service contexts).\n"
    "    if ($env:HOME) { $p = $p -replace [regex]::Escape($env:HOME), '~' }\n"
    "    elseif ($env:USERPROFILE) { $p = $p -replace [regex]::Escape($env:USERPROFILE), '~' }\n"
    "\n"
    "    $sigil = if ($script:VrtxTerm_IsAdmin) { \"$red#$rst\" } else { '$' }\n"
    "\n"
    "    \"$cyan\u256d\u2500$rst$grn$u@$h$rst  $blu$p$rst`n$cyan\u2570\u2500$rst$sigil \"\n"
    "}\n";

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

    const std::wstring dir = base + L"\\VrtxTerm";
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

}  // namespace vrtx::terminal
