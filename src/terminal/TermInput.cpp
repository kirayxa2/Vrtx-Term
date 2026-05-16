#include "terminal/TermInput.h"

#include <cstring>

namespace vrtx::terminal {

namespace {

// xterm "modifier value" encoded into CSI sequences: 1 + bitmask, where
// bit0=shift, bit1=alt, bit2=ctrl. So plain shift = 2, alt = 3, shift+alt = 4,
// ctrl = 5, etc. Used inside CSI 1;<m><FINAL> for arrows / Home / End.
int ModParam(uint8_t mods) {
    int m = 1;
    if (mods & mod::kShift) m += 1;
    if (mods & mod::kAlt)   m += 2;
    if (mods & mod::kCtrl)  m += 4;
    return m;
}

// Helper: write either "ESC [ <final>" (no mods) or "ESC [ 1 ; <m> <final>".
size_t EmitCsiArrow(char final_, uint8_t mods, char* out, size_t cap) {
    if (mods == 0) {
        if (cap < 3) return 0;
        out[0] = 0x1B; out[1] = '[';  out[2] = final_;
        return 3;
    }
    return static_cast<size_t>(
        std::snprintf(out, cap, "\x1b[1;%d%c", ModParam(mods), final_));
}

// Same idea but for the "tilde-style" keys: Home/End/Insert/Delete/PgUp/PgDn,
// F1..F12. Format: ESC [ <num> ~  or  ESC [ <num> ; <m> ~ when modified.
size_t EmitCsiTilde(int num, uint8_t mods, char* out, size_t cap) {
    if (mods == 0) {
        return static_cast<size_t>(std::snprintf(out, cap, "\x1b[%d~", num));
    }
    return static_cast<size_t>(
        std::snprintf(out, cap, "\x1b[%d;%d~", num, ModParam(mods)));
}

// F1..F4 use the "SS3" form: ESC O P / Q / R / S, no number.
size_t EmitSs3(char final_, uint8_t mods, char* out, size_t cap) {
    if (mods == 0) {
        if (cap < 3) return 0;
        out[0] = 0x1B; out[1] = 'O'; out[2] = final_;
        return 3;
    }
    // Modified F1..F4 are normally encoded the CSI way.
    return static_cast<size_t>(
        std::snprintf(out, cap, "\x1b[1;%d%c", ModParam(mods), final_));
}

}  // namespace

// ---------------------------------------------------------------------------

uint8_t CurrentModifiers() {
    uint8_t m = 0;
    if (::GetKeyState(VK_SHIFT)   & 0x8000) m |= mod::kShift;
    if (::GetKeyState(VK_MENU)    & 0x8000) m |= mod::kAlt;
    if (::GetKeyState(VK_CONTROL) & 0x8000) m |= mod::kCtrl;
    return m;
}

// ---------------------------------------------------------------------------

size_t TranslateVirtualKey(WPARAM vk, uint8_t mods, char* out, size_t cap) {
    if (cap < 16) return 0;

    switch (vk) {
        // ---- Arrows ----
        case VK_UP:    return EmitCsiArrow('A', mods, out, cap);
        case VK_DOWN:  return EmitCsiArrow('B', mods, out, cap);
        case VK_RIGHT: return EmitCsiArrow('C', mods, out, cap);
        case VK_LEFT:  return EmitCsiArrow('D', mods, out, cap);

        // ---- Home / End ----
        case VK_HOME:  return EmitCsiArrow('H', mods, out, cap);
        case VK_END:   return EmitCsiArrow('F', mods, out, cap);

        // ---- Page Up / Down / Insert / Delete ----
        case VK_PRIOR:  return EmitCsiTilde(5, mods, out, cap);
        case VK_NEXT:   return EmitCsiTilde(6, mods, out, cap);
        case VK_INSERT: return EmitCsiTilde(2, mods, out, cap);
        case VK_DELETE: return EmitCsiTilde(3, mods, out, cap);

        // ---- F1..F12 ----
        case VK_F1: return EmitSs3('P', mods, out, cap);
        case VK_F2: return EmitSs3('Q', mods, out, cap);
        case VK_F3: return EmitSs3('R', mods, out, cap);
        case VK_F4: return EmitSs3('S', mods, out, cap);
        case VK_F5:  return EmitCsiTilde(15, mods, out, cap);
        case VK_F6:  return EmitCsiTilde(17, mods, out, cap);
        case VK_F7:  return EmitCsiTilde(18, mods, out, cap);
        case VK_F8:  return EmitCsiTilde(19, mods, out, cap);
        case VK_F9:  return EmitCsiTilde(20, mods, out, cap);
        case VK_F10: return EmitCsiTilde(21, mods, out, cap);
        case VK_F11: return EmitCsiTilde(23, mods, out, cap);
        case VK_F12: return EmitCsiTilde(24, mods, out, cap);

        // ---- Backspace ----
        // Send 0x7F (DEL) for a plain backspace, the de-facto xterm/Linux
        // convention; PowerShell's PSReadLine and most line editors expect
        // it. Ctrl+Backspace -> 0x08 (delete-prev-word in many shells).
        case VK_BACK:
            if (mods & mod::kCtrl) { out[0] = 0x08; return 1; }
            out[0] = 0x7F;
            return 1;

        // ---- Tab / Shift+Tab ----
        case VK_TAB:
            if (mods & mod::kShift) {
                static constexpr char kBackTab[] = "\x1b[Z";
                std::memcpy(out, kBackTab, sizeof(kBackTab) - 1);
                return sizeof(kBackTab) - 1;
            }
            out[0] = '\t';
            return 1;

        // ---- Escape ----
        case VK_ESCAPE:
            out[0] = 0x1B;
            return 1;

        // ---- Enter ----
        case VK_RETURN:
            // Alt+Enter -> ESC \r (xterm meta-Enter).
            if (mods & mod::kAlt) { out[0] = 0x1B; out[1] = '\r'; return 2; }
            out[0] = '\r';
            return 1;

        default: break;
    }

    // ---- Ctrl + letter -> 0x01 .. 0x1A (Ctrl+A == ^A == 0x01 etc.) -------
    //
    // Letters/numbers also produce WM_CHAR; we intercept Ctrl+letter here so
    // the shell receives the canonical control byte instead of whatever
    // localized WM_CHAR Windows produces. We do NOT intercept plain letters,
    // those are handled by TranslateChar via WM_CHAR.
    if ((mods & mod::kCtrl) && !(mods & mod::kAlt) &&
        vk >= 'A' && vk <= 'Z') {
        out[0] = static_cast<char>(vk - 'A' + 1);
        return 1;
    }
    if ((mods & mod::kCtrl) && !(mods & mod::kAlt)) {
        switch (vk) {
            case VK_SPACE:   out[0] = 0x00; return 1;   // Ctrl+Space -> NUL
            case 0xDB:       out[0] = 0x1B; return 1;   // Ctrl+[      -> ESC
            case 0xDC:       out[0] = 0x1C; return 1;   // Ctrl+\
            case 0xDD:       out[0] = 0x1D; return 1;   // Ctrl+]
            default: break;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------

size_t TranslateChar(wchar_t ch, char* out, size_t cap) {
    // Surrogate pairs: Windows delivers them as two consecutive WM_CHARs.
    static thread_local wchar_t pending_high = 0;

    char32_t cp = 0;
    if (ch >= 0xD800 && ch <= 0xDBFF) {
        pending_high = ch;
        return 0;
    }
    if (ch >= 0xDC00 && ch <= 0xDFFF) {
        if (pending_high == 0) return 0;
        cp = 0x10000 + ((static_cast<char32_t>(pending_high) - 0xD800) << 10)
                     +  (static_cast<char32_t>(ch)            - 0xDC00);
        pending_high = 0;
    } else {
        pending_high = 0;
        cp = ch;
    }

    // Drop control bytes that aren't synthesised "for fun" by TranslateMessage
    // for Ctrl+letter (^A == 0x01 etc.) - those are handled in the VK path
    // already, and re-sending them here would double the keystroke.
    //
    // We also drop CR / LF / TAB / BS for the same reason: VK_RETURN /
    // VK_TAB / VK_BACK already produced the canonical bytes via the VK path,
    // so the WM_CHAR that TranslateMessage queues right after must be ignored.
    if (cp < 0x20)   return 0;
    if (cp == 0x7F)  return 0;   // VK_BACK already produced 0x7F.

    // UTF-8 encode.
    if (cp < 0x80) {
        if (cap < 1) return 0;
        out[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        if (cap < 2) return 0;
        out[0] = static_cast<char>(0xC0 | (cp >> 6));
        out[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        if (cap < 3) return 0;
        out[0] = static_cast<char>(0xE0 |  (cp >> 12));
        out[1] = static_cast<char>(0x80 | ((cp >>  6) & 0x3F));
        out[2] = static_cast<char>(0x80 |  (cp        & 0x3F));
        return 3;
    }
    if (cap < 4) return 0;
    out[0] = static_cast<char>(0xF0 |  (cp >> 18));
    out[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((cp >>  6) & 0x3F));
    out[3] = static_cast<char>(0x80 |  (cp        & 0x3F));
    return 4;
}

}  // namespace vrtx::terminal
