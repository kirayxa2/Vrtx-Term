// Default PowerShell profile for VrtxTerm.
//
// We don't want the user's existing PowerShell profile (oh-my-posh,
// PSReadLine themes, winfetch on launch, etc.) to run inside our window
// because (a) prompts based on Powerline / Nerd Fonts depend on whatever
// font the user picked elsewhere and (b) we want a deterministic, clean
// look that matches the native VrtxTerm look out of the box.
//
// Solution: ship our own tiny `.ps1` file that defines the prompt we
// want, write it to %LOCALAPPDATA%\VrtxTerm\profile.ps1 on first run,
// and launch pwsh with `-NoLogo -NoProfile -NoExit -File "<path>"`.
// `-NoProfile` skips $PROFILE entirely; `-File` runs ours instead. The
// shell then drops into an interactive REPL with our prompt active.

#pragma once

#include "pch.h"

namespace vrtx::terminal {

// Materialise the default profile on disk (creating the directory tree
// if needed) and return its absolute path. Overwrites any existing file
// so updates to the embedded template propagate on next launch.
//
// Returns an empty string only if %LOCALAPPDATA% is unreachable, which
// shouldn't happen on any sane Windows account.
std::wstring EnsureDefaultPwshProfile();

}  // namespace vrtx::terminal
