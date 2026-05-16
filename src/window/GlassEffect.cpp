#include "window/GlassEffect.h"

namespace vrtx::window {

namespace {

// ---- Undocumented SetWindowCompositionAttribute interop --------------------
//
// These structs and enums live in user32.dll but are not in the public SDK
// headers. The layout has been stable since Windows 10 1803.

enum AccentState : int {
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND   = 4,
    ACCENT_ENABLE_HOSTBACKDROP        = 5,
    ACCENT_INVALID_STATE              = 6,
};

struct AccentPolicy {
    AccentState accentState;
    int         accentFlags;
    uint32_t    gradientColor;     // ABGR
    int         animationId;
};

enum WindowCompositionAttribute : int {
    WCA_ACCENT_POLICY = 19,
};

struct WindowCompositionAttributeData {
    WindowCompositionAttribute attribute;
    void*                      data;
    size_t                     dataSize;
};

using PFN_SetWindowCompositionAttribute =
    BOOL(WINAPI*)(HWND, WindowCompositionAttributeData*);

PFN_SetWindowCompositionAttribute LoadSetCompositionAttr() {
    static PFN_SetWindowCompositionAttribute fn = []() {
        const HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
        if (!user32) return PFN_SetWindowCompositionAttribute{nullptr};
        return reinterpret_cast<PFN_SetWindowCompositionAttribute>(
            ::GetProcAddress(user32, "SetWindowCompositionAttribute"));
    }();
    return fn;
}

}  // namespace

bool ApplyAcrylicBackdrop(HWND hwnd, uint32_t accentColorABGR) {
    const auto fn = LoadSetCompositionAttr();
    if (!fn) {
        return false;
    }

    AccentPolicy policy{};
    policy.accentState   = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.accentFlags   = 0x20 | 0x40 | 0x80 | 0x100;  // draw all four borders
    policy.gradientColor = accentColorABGR;
    policy.animationId   = 0;

    WindowCompositionAttributeData data{};
    data.attribute = WCA_ACCENT_POLICY;
    data.data      = &policy;
    data.dataSize  = sizeof(policy);

    return fn(hwnd, &data) != FALSE;
}

void RemoveBackdrop(HWND hwnd) {
    const auto fn = LoadSetCompositionAttr();
    if (!fn) return;

    AccentPolicy policy{};
    policy.accentState = ACCENT_DISABLED;

    WindowCompositionAttributeData data{};
    data.attribute = WCA_ACCENT_POLICY;
    data.data      = &policy;
    data.dataSize  = sizeof(policy);

    fn(hwnd, &data);
}

}  // namespace vrtx::window
