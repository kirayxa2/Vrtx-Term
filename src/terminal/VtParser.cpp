#include "terminal/VtParser.h"

#include <cstring>

namespace vrtx::terminal {

namespace {

constexpr size_t kMaxOscLen   = 4096;
constexpr size_t kMaxCsiParams = 16;

// xterm 256-color table indices 16..231: 6x6x6 cube, 232..255: greyscale.
// We map them down to RGB so the renderer doesn't need a 256-entry table.
CellColor MakeIndexed256(int idx) {
    if (idx < 0)   idx = 0;
    if (idx > 255) idx = 255;
    if (idx < 16) {
        return CellColor::MakeIndexed(static_cast<uint8_t>(idx));
    }
    if (idx < 232) {
        const int v   = idx - 16;
        const int r   = (v / 36) % 6;
        const int g   = (v / 6)  % 6;
        const int b   =  v       % 6;
        auto step = [](int n) -> uint8_t {
            // xterm-canonical levels: 0, 95, 135, 175, 215, 255.
            static constexpr uint8_t kLevels[6] = {0, 95, 135, 175, 215, 255};
            return kLevels[n];
        };
        return CellColor::MakeRgb(step(r), step(g), step(b));
    }
    const uint8_t v = static_cast<uint8_t>(8 + (idx - 232) * 10);
    return CellColor::MakeRgb(v, v, v);
}

}  // namespace

// ---------------------------------------------------------------------------

void VtParser::Feed(const char* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        StepByte(static_cast<uint8_t>(data[i]));
    }
}

// ---- UTF-8 decoding -------------------------------------------------------
//
// Standalone tiny decoder. Only printables make it to OnPrint; malformed
// bytes are replaced with U+FFFD.

void VtParser::Utf8Push(uint8_t b) {
    if (utf8_remaining_ == 0) {
        if (b < 0x80) {
            OnPrint(static_cast<char32_t>(b));
        } else if ((b & 0xE0) == 0xC0) {
            utf8_acc_       = b & 0x1F;
            utf8_remaining_ = 1;
        } else if ((b & 0xF0) == 0xE0) {
            utf8_acc_       = b & 0x0F;
            utf8_remaining_ = 2;
        } else if ((b & 0xF8) == 0xF0) {
            utf8_acc_       = b & 0x07;
            utf8_remaining_ = 3;
        } else {
            OnPrint(0xFFFD);
        }
    } else {
        if ((b & 0xC0) != 0x80) {
            // Resync: dropped continuation, restart on this byte.
            Utf8Reset();
            Utf8Push(b);
            return;
        }
        utf8_acc_ = (utf8_acc_ << 6) | (b & 0x3F);
        if (--utf8_remaining_ == 0) {
            OnPrint(utf8_acc_);
        }
    }
}

// ---- State machine --------------------------------------------------------

void VtParser::StepByte(uint8_t b) {
    // Anywhere transitions per the spec: ESC always restarts; CAN/SUB abort.
    if (b == 0x1B) {                         // ESC
        Utf8Reset();
        ResetCsi();
        osc_buf_.clear();
        state_ = State::Escape;
        return;
    }
    if (b == 0x18 || b == 0x1A) {            // CAN / SUB
        Utf8Reset();
        state_ = State::Ground;
        return;
    }

    switch (state_) {
        case State::Ground:
            if (b < 0x20 || b == 0x7F) {
                OnExecute(b);
            } else {
                Utf8Push(b);
            }
            break;

        case State::Escape:
            if (b >= 0x20 && b <= 0x2F) {
                esc_intermediate_ = b;
                state_ = State::EscIntermediate;
            } else if (b == '[') {
                ResetCsi();
                state_ = State::CsiEntry;
            } else if (b == ']') {
                osc_buf_.clear();
                state_ = State::OscString;
            } else if (b == 'P' || b == 'X' || b == '^' || b == '_') {
                // DCS / SOS / PM / APC -> swallow until ST (ESC \) or BEL.
                state_ = State::DcsIgnore;
            } else if (b >= 0x30 && b <= 0x7E) {
                OnEscDispatch(b);
                state_ = State::Ground;
            } else if (b < 0x20) {
                // C0 inside ESC -> execute, stay in Escape.
                OnExecute(b);
            } else {
                state_ = State::Ground;
            }
            break;

        case State::EscIntermediate:
            // Most commonly ESC ( B  (G0 = US-ASCII). We swallow the final
            // byte and return to Ground; we only support UTF-8 anyway.
            if (b >= 0x30 && b <= 0x7E) {
                state_ = State::Ground;
            }
            break;

        case State::CsiEntry:
            if (b >= 0x30 && b <= 0x39) {                  // digit
                csi_current_     = b - '0';
                csi_has_current_ = true;
                state_           = State::CsiParam;
            } else if (b == ';' || b == ':') {
                csi_params_.push_back(0);
                state_ = State::CsiParam;
            } else if (b >= 0x3C && b <= 0x3F) {           // < = > ?
                csi_private_ = b;
                state_       = State::CsiParam;
            } else if (b >= 0x20 && b <= 0x2F) {
                csi_intermediate_ = b;
                state_            = State::CsiIntermediate;
            } else if (b >= 0x40 && b <= 0x7E) {
                OnCsiDispatch(b);
                state_ = State::Ground;
            } else if (b < 0x20) {
                OnExecute(b);
            } else {
                state_ = State::CsiIgnore;
            }
            break;

        case State::CsiParam:
            if (b >= 0x30 && b <= 0x39) {
                csi_current_     = csi_current_ * 10 + (b - '0');
                csi_has_current_ = true;
            } else if (b == ';' || b == ':') {
                if (csi_params_.size() < kMaxCsiParams) {
                    csi_params_.push_back(csi_has_current_ ? csi_current_ : 0);
                }
                csi_current_     = 0;
                csi_has_current_ = false;
            } else if (b >= 0x20 && b <= 0x2F) {
                if (csi_has_current_ && csi_params_.size() < kMaxCsiParams) {
                    csi_params_.push_back(csi_current_);
                }
                csi_intermediate_ = b;
                state_            = State::CsiIntermediate;
            } else if (b >= 0x40 && b <= 0x7E) {
                if (csi_has_current_ && csi_params_.size() < kMaxCsiParams) {
                    csi_params_.push_back(csi_current_);
                }
                OnCsiDispatch(b);
                state_ = State::Ground;
            } else if (b < 0x20) {
                OnExecute(b);
            } else {
                state_ = State::CsiIgnore;
            }
            break;

        case State::CsiIntermediate:
            if (b >= 0x40 && b <= 0x7E) {
                OnCsiDispatch(b);
                state_ = State::Ground;
            } else if (b < 0x20) {
                OnExecute(b);
            }
            // else: keep eating intermediates.
            break;

        case State::CsiIgnore:
            if (b >= 0x40 && b <= 0x7E) {
                state_ = State::Ground;
            }
            break;

        case State::OscString:
            if (b == 0x07) {                                // BEL terminator
                OnOscDispatch();
                state_ = State::Ground;
            } else if (b == 0x9C) {                         // ST in C1
                OnOscDispatch();
                state_ = State::Ground;
            } else if (b == 0x1B) {
                // Already handled above; never reaches here.
            } else {
                if (osc_buf_.size() < kMaxOscLen)
                    osc_buf_.push_back(static_cast<char>(b));
            }
            break;

        case State::DcsIgnore:
            // Wait for ST (ESC \). The ESC handler above resets us; we just
            // need to consume the trailing '\' -> done.
            if (b == '\\') {
                state_ = State::Ground;
            } else if (b == 0x07 || b == 0x9C) {
                state_ = State::Ground;
            }
            break;
    }
}

// ---- CSI helpers ----------------------------------------------------------

void VtParser::ResetCsi() {
    csi_params_.clear();
    csi_current_     = 0;
    csi_has_current_ = false;
    csi_private_     = 0;
    csi_intermediate_= 0;
}

int VtParser::Param(size_t i, int defaultVal) const {
    if (i < csi_params_.size() && csi_params_[i] != 0) return csi_params_[i];
    if (i < csi_params_.size())                         return csi_params_[i];
    return defaultVal;
}

// ---- Dispatch entry points ------------------------------------------------

void VtParser::OnPrint(char32_t cp) {
    buf_.PutChar(cp);
}

void VtParser::OnExecute(uint8_t b) {
    switch (b) {
        case '\r': buf_.CarriageReturn(); break;
        case '\n':
        case 0x0B:    // VT
        case 0x0C:    // FF
            buf_.LineFeed(); break;
        case '\b': buf_.Backspace(); break;
        case '\t': buf_.Tab(); break;
        case 0x07: /* BEL: TODO bell */ break;
        default: break;
    }
}

void VtParser::OnEscDispatch(uint8_t finalByte) {
    switch (finalByte) {
        case '7': buf_.SaveCursor();    break;   // DECSC
        case '8': buf_.RestoreCursor(); break;   // DECRC
        case 'D': buf_.LineFeed();      break;   // IND
        case 'E':                                  // NEL
            buf_.CarriageReturn();
            buf_.LineFeed();
            break;
        case 'M':                                  // RI - reverse index
            if (buf_.CursorRow() == buf_.ScrollTop()) {
                buf_.ScrollDown(1);
            } else {
                buf_.SetCursor(buf_.CursorRow() - 1, buf_.CursorCol());
            }
            break;
        case 'c':                                  // RIS - full reset
            buf_.ResetSgr();
            buf_.SetCursor(0, 0);
            buf_.EraseInDisplay(2);
            break;
        default: break;
    }
}

// ---- OSC ------------------------------------------------------------------
//
// We accept OSC `0;<text>` (icon name + window title) and `2;<text>`
// (window title) and just ignore the payload. Window-title plumbing into
// the chrome is on the TODO list. Other OSC numbers (color queries, link
// hyperlinks, clipboard) get dropped on purpose for the MVP.

void VtParser::OnOscDispatch() {
    // No-op for now; accept-and-drop. Caller has already accumulated osc_buf_.
    osc_buf_.clear();
}

// ---- CSI dispatch ---------------------------------------------------------

void VtParser::OnCsiDispatch(uint8_t finalByte) {
    // Convenience: clamp 1-based to 0-based row/col.
    auto rowParam = [&](size_t i) { return std::max(1, Param(i, 1)) - 1; };
    auto colParam = [&](size_t i) { return std::max(1, Param(i, 1)) - 1; };
    auto count1   = [&](size_t i) { return std::max(1, Param(i, 1)); };

    // Private (DEC) sequences: CSI ? ... h / l
    if (csi_private_ == '?') {
        if (finalByte == 'h') { HandleDecSet(true);  return; }
        if (finalByte == 'l') { HandleDecSet(false); return; }
        // Other private sequences: ignore.
        return;
    }
    // CSI > q (xterm secondary DA) and friends: ignore.
    if (csi_private_ != 0) {
        return;
    }

    switch (finalByte) {
        // --- Cursor movement ---
        case 'A': buf_.SetCursor(buf_.CursorRow() - count1(0), buf_.CursorCol()); break;  // CUU
        case 'B': buf_.SetCursor(buf_.CursorRow() + count1(0), buf_.CursorCol()); break;  // CUD
        case 'C': buf_.SetCursor(buf_.CursorRow(), buf_.CursorCol() + count1(0)); break;  // CUF
        case 'D': buf_.SetCursor(buf_.CursorRow(), buf_.CursorCol() - count1(0)); break;  // CUB
        case 'E':                                                                          // CNL
            buf_.SetCursor(buf_.CursorRow() + count1(0), 0);
            break;
        case 'F':                                                                          // CPL
            buf_.SetCursor(buf_.CursorRow() - count1(0), 0);
            break;
        case 'G':                                                                          // CHA
            buf_.SetCursor(buf_.CursorRow(), colParam(0));
            break;
        case 'H':                                                                          // CUP
        case 'f':                                                                          // HVP
            buf_.SetCursor(rowParam(0), colParam(1));
            break;
        case 'd':                                                                          // VPA
            buf_.SetCursor(rowParam(0), buf_.CursorCol());
            break;

        // --- Erase ---
        case 'J': buf_.EraseInDisplay(Param(0, 0)); break;
        case 'K': buf_.EraseInLine   (Param(0, 0)); break;
        case 'L': buf_.InsertLines (count1(0));    break;
        case 'M': buf_.DeleteLines (count1(0));    break;
        case 'P': buf_.DeleteChars (count1(0));    break;
        case '@': buf_.InsertChars (count1(0));    break;
        case 'X': buf_.EraseChars  (count1(0));    break;

        // --- Scroll ---
        case 'S': buf_.ScrollUp  (count1(0)); break;
        case 'T': buf_.ScrollDown(count1(0)); break;

        // --- Scroll region (DECSTBM) ---
        case 'r': {
            const int top    = Param(0, 1) - 1;
            const int bottom = Param(1, buf_.Rows()) - 1;
            buf_.SetScrollRegion(top, bottom);
            break;
        }

        // --- Cursor save / restore (ANSI variant) ---
        case 's': buf_.SaveCursor();    break;
        case 'u': buf_.RestoreCursor(); break;

        // --- SGR ---
        case 'm': HandleSgr(); break;

        // --- Device status report (CPR) ---
        case 'n':
            if (Param(0, 0) == 6 && write_back_) {
                char reply[32];
                int n = std::snprintf(reply, sizeof(reply), "\x1b[%d;%dR",
                                      buf_.CursorRow() + 1,
                                      buf_.CursorCol() + 1);
                if (n > 0) write_back_(reply, static_cast<size_t>(n));
            }
            break;

        // --- Primary device attributes ---
        case 'c':
            if (write_back_) {
                // Respond as a VT100 with AVO ("\e[?1;2c").
                static constexpr char kReply[] = "\x1b[?1;2c";
                write_back_(kReply, sizeof(kReply) - 1);
            }
            break;

        default:
            break;
    }
}

// ---- DECSET / DECRST (CSI ? ... h / l) ------------------------------------

void VtParser::HandleDecSet(bool enable) {
    for (int p : csi_params_) {
        switch (p) {
            case 25:                 // DECTCEM - cursor visible.
                // Renderer always draws cursor; we ignore.
                break;
            case 47:                 // alt screen (legacy)
            case 1047:
                if (enable) buf_.SwitchToAltScreen();
                else        buf_.SwitchToMainScreen();
                break;
            case 1048:               // save / restore cursor
                if (enable) buf_.SaveCursor();
                else        buf_.RestoreCursor();
                break;
            case 1049:               // alt screen + save cursor + clear
                if (enable) {
                    buf_.SaveCursor();
                    buf_.SwitchToAltScreen();
                    buf_.SetCursor(0, 0);
                    buf_.EraseInDisplay(2);
                } else {
                    buf_.SwitchToMainScreen();
                    buf_.RestoreCursor();
                }
                break;
            default:
                // Bracketed paste (2004), focus reporting (1004), mouse
                // tracking (1000/1002/1006), etc. - silently ignored.
                break;
        }
    }
}

// ---- SGR ------------------------------------------------------------------
//
// Supports:
//   0           reset
//   1/2/3/4/7/9 attrs (bold, faint, italic, underline, reverse, strike)
//   22/23/24/27/29 attr-off
//   30..37, 90..97  fg indexed
//   40..47, 100..107 bg indexed
//   38;5;N        fg 256-color
//   48;5;N        bg 256-color
//   38;2;R;G;B    fg truecolor
//   48;2;R;G;B    bg truecolor
//   39 / 49       fg/bg default

void VtParser::HandleSgr() {
    if (csi_params_.empty()) {
        buf_.ResetSgr();
        return;
    }

    for (size_t i = 0; i < csi_params_.size(); ++i) {
        const int p = csi_params_[i];
        switch (p) {
            case 0:  buf_.ResetSgr();                        break;
            case 1:  buf_.SetAttr  (attr::kBold);            break;
            case 2:  buf_.SetAttr  (attr::kFaint);           break;
            case 3:  buf_.SetAttr  (attr::kItalic);          break;
            case 4:  buf_.SetAttr  (attr::kUnderline);       break;
            case 7:  buf_.SetAttr  (attr::kReverse);         break;
            case 9:  buf_.SetAttr  (attr::kStrike);          break;
            case 21:
            case 22: buf_.ClearAttr(attr::kBold | attr::kFaint); break;
            case 23: buf_.ClearAttr(attr::kItalic);          break;
            case 24: buf_.ClearAttr(attr::kUnderline);       break;
            case 27: buf_.ClearAttr(attr::kReverse);         break;
            case 29: buf_.ClearAttr(attr::kStrike);          break;

            case 39: buf_.SetFg(CellColor::MakeDefault());   break;
            case 49: buf_.SetBg(CellColor::MakeDefault());   break;

            case 38:
            case 48: {
                // Extended color: need at least one more param.
                if (i + 1 >= csi_params_.size()) return;
                const int mode = csi_params_[i + 1];
                if (mode == 5 && i + 2 < csi_params_.size()) {
                    auto col = MakeIndexed256(csi_params_[i + 2]);
                    if (p == 38) buf_.SetFg(col); else buf_.SetBg(col);
                    i += 2;
                } else if (mode == 2 && i + 4 < csi_params_.size()) {
                    auto col = CellColor::MakeRgb(
                        static_cast<uint8_t>(csi_params_[i + 2] & 0xFF),
                        static_cast<uint8_t>(csi_params_[i + 3] & 0xFF),
                        static_cast<uint8_t>(csi_params_[i + 4] & 0xFF));
                    if (p == 38) buf_.SetFg(col); else buf_.SetBg(col);
                    i += 4;
                } else {
                    return;
                }
                break;
            }

            default:
                if (p >= 30 && p <= 37)
                    buf_.SetFg(CellColor::MakeIndexed(static_cast<uint8_t>(p - 30)));
                else if (p >= 40 && p <= 47)
                    buf_.SetBg(CellColor::MakeIndexed(static_cast<uint8_t>(p - 40)));
                else if (p >= 90 && p <= 97)
                    buf_.SetFg(CellColor::MakeIndexed(static_cast<uint8_t>(8 + p - 90)));
                else if (p >= 100 && p <= 107)
                    buf_.SetBg(CellColor::MakeIndexed(static_cast<uint8_t>(8 + p - 100)));
                break;
        }
    }
}

}  // namespace vrtx::terminal
