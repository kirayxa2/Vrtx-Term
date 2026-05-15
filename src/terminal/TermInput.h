// Keystroke -> VT byte translation.
//
// Two entry points:
//
//   TranslateVirtualKey  - handles WM_KEYDOWN. Looks at the virtual-key code
//                          plus shift/ctrl/alt state and returns the VT
//                          escape sequence (xterm-flavoured) for arrows,
//                          F1-F12, Home/End, PgUp/PgDn, Insert/Delete, plus
//                          Ctrl+letter -> 0x01..0x1A.
//
//   TranslateChar        - handles WM_CHAR. UTF-16 wchar_t in (Windows
//                          delivers WM_CHAR as UTF-16 under UNICODE), UTF-8
//                          bytes out.
//
// The only state we need is the modifier mask, which the caller packs from
// the current keyboard state at call time. That keeps this translator a
// pure function and easy to unit-test later.

#pragma once

#include "pch.h"

namespace mactw::terminal {

namespace mod {
inline constexpr uint8_t kShift = 1u << 0;
inline constexpr uint8_t kAlt   = 1u << 1;
inline constexpr uint8_t kCtrl  = 1u << 2;
}  // namespace mod

// Build a modifier mask from the current async-key-state. Cheap; safe to
// call inside WM_KEYDOWN.
uint8_t CurrentModifiers();

// Translate a WM_KEYDOWN. Writes 0..N bytes into `out` (size <= 16). Returns
// the byte count, or 0 if the key has no VT representation here (caller
// should let WM_CHAR handle it via TranslateMessage).
size_t TranslateVirtualKey(WPARAM vk, uint8_t mods, char* out, size_t cap);

// Translate a WM_CHAR (a UTF-16 code unit). Encodes one code point into
// out[] as UTF-8. High surrogates are buffered between calls via a static
// thread-local; this matches how Windows delivers astral planes.
size_t TranslateChar(wchar_t ch, char* out, size_t cap);

}  // namespace mactw::terminal
