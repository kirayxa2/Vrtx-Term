#include "theme/RuntimeTweaks.h"
#include "theme/TahoeTheme.h"

namespace mactw::theme::tweaks {

float gCornerRadius  = kWindowCornerRadius;     // 16 pt
float gTlDiameter    = kTrafficLightDiameter;   // 12 pt (Apple HIG canonical)
float gTlSpacing     = kTrafficLightSpacing;    //  8 pt
float gTlInsetX      = kTrafficLightInsetX;     // 14 pt (so center is 20pt from edge)
float gCaptionHeight = kCaptionHeight;          // 30 pt

namespace {

float Bump(float value, bool shift, int dir, float lo, float hi) {
    const float step = (shift ? 4.0f : 1.0f) * static_cast<float>(dir);
    return std::clamp(value + step, lo, hi);
}

}  // namespace

bool HandleKey(WPARAM vk, bool shift) {
    // VK_OEM_4 = '['     VK_OEM_6  = ']'
    // VK_OEM_COMMA = ','  VK_OEM_PERIOD = '.'
    // VK_OEM_1 = ';'      VK_OEM_7  = '\''
    // VK_OEM_MINUS = '-'  VK_OEM_PLUS = '='

    switch (vk) {
        case VK_OEM_4:           gCornerRadius  = Bump(gCornerRadius,  shift, -1, 0,  64); return true;
        case VK_OEM_6:           gCornerRadius  = Bump(gCornerRadius,  shift, +1, 0,  64); return true;
        case VK_OEM_COMMA:       gTlDiameter    = Bump(gTlDiameter,    shift, -1, 4,  24); return true;
        case VK_OEM_PERIOD:      gTlDiameter    = Bump(gTlDiameter,    shift, +1, 4,  24); return true;
        case VK_OEM_1:           gTlSpacing     = Bump(gTlSpacing,     shift, -1, 0,  20); return true;
        case VK_OEM_7:           gTlSpacing     = Bump(gTlSpacing,     shift, +1, 0,  20); return true;
        case VK_OEM_MINUS:       gTlInsetX      = Bump(gTlInsetX,      shift, -1, 0,  40); return true;
        case VK_OEM_PLUS:        gTlInsetX      = Bump(gTlInsetX,      shift, +1, 0,  40); return true;
        case '9':                gCaptionHeight = Bump(gCaptionHeight, shift, -1, 12, 60); return true;
        case '0':                gCaptionHeight = Bump(gCaptionHeight, shift, +1, 12, 60); return true;
        default:                 return false;
    }
}

}  // namespace mactw::theme::tweaks
