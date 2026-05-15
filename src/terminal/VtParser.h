// VT100 / xterm escape-sequence parser.
//
// State machine follows Paul Williams' canonical diagram for the DEC ANSI
// parser (https://vt100.net/emu/dec_ansi_parser). We implement just the
// states needed for PowerShell, ls/git colors, vim, htop, oh-my-posh:
//
//   * GROUND          - regular printable bytes -> PutChar
//   * ESCAPE          - saw ESC, expecting next byte
//   * CSI_ENTRY       - saw ESC [ - reading parameters
//   * CSI_PARAM       - accumulating digits / ';' separators
//   * CSI_INTERMEDIATE- private-marker / intermediate bytes (e.g. ?, !, $)
//   * CSI_IGNORE      - error; drop until final byte
//   * OSC_STRING      - saw ESC ] - read until BEL or ST (ESC \)
//
// Out-of-scope: DCS/SOS/PM/APC sequences (we drop them silently), single-
// shifts (SS2/SS3), G0/G1 character set switching, complex DCS responses.
//
// The parser is push-driven: feed raw pty bytes to Feed(); it calls into
// TermBuffer for grid mutations and into a "WriteBack" callback for replies
// (CPR cursor reports, DA device-attributes, etc.). The TermBuffer mutex
// must be held for the whole Feed() call by the caller.

#pragma once

#include "pch.h"
#include "terminal/TermBuffer.h"

#include <functional>

namespace mactw::terminal {

// Callback that sends bytes back to the pty (DSR / DA replies).
using WriteBackFn = std::function<void(const char* data, size_t len)>;

class VtParser {
public:
    explicit VtParser(TermBuffer& buf) : buf_(buf) {}

    void SetWriteBack(WriteBackFn fn) { write_back_ = std::move(fn); }

    // Feed raw bytes (UTF-8) coming out of the pseudo-console. Caller must
    // hold buf_.Lock() for the duration of the call.
    void Feed(const char* data, size_t len);

private:
    enum class State : uint8_t {
        Ground,
        Escape,
        EscIntermediate,
        CsiEntry,
        CsiParam,
        CsiIntermediate,
        CsiIgnore,
        OscString,
        DcsIgnore,    // ESC P ... ST  -> ignored
    };

    void StepByte(uint8_t b);
    void OnPrint(char32_t cp);
    void OnExecute(uint8_t b);            // C0 control character
    void OnCsiDispatch(uint8_t finalByte);
    void OnEscDispatch(uint8_t finalByte);
    void OnOscDispatch();                 // called when OSC ends

    void ResetCsi();
    int  Param(size_t i, int defaultVal) const;

    void HandleSgr();                     // CSI ... m
    void HandleDecSet(bool enable);       // CSI ? ... h / l

    bool IsCsiPrivate() const { return csi_private_ != 0; }

    // ---- UTF-8 decode -----------------------------------------------------
    void Utf8Push(uint8_t b);
    void Utf8Reset() { utf8_remaining_ = 0; utf8_acc_ = 0; }

    TermBuffer&  buf_;
    WriteBackFn  write_back_;

    State state_{State::Ground};

    // CSI accumulators
    std::vector<int> csi_params_;
    int              csi_current_{0};
    bool             csi_has_current_{false};
    uint8_t          csi_private_{0};       // '?', '>', '=' etc.
    uint8_t          csi_intermediate_{0};  // first intermediate byte; rare.

    // OSC accumulator (capped to avoid OOM on malicious input)
    std::string      osc_buf_;

    // ESC intermediate (e.g. ESC ( B for char-set selection - we just eat it)
    uint8_t          esc_intermediate_{0};

    // UTF-8 decoder
    int              utf8_remaining_{0};
    char32_t         utf8_acc_{0};
};

}  // namespace mactw::terminal
