#include "theme/RuntimeTweaks.h"
#include "theme/TahoeTheme.h"

namespace mactw::theme::tweaks {

float gCornerRadius = kWindowCornerRadius;  // 26 pt (Apple toolbar window)

bool HandleKey(WPARAM vk, bool shift) {
    const float step = shift ? 4.0f : 1.0f;

    // VK_OEM_4 = '[', VK_OEM_6 = ']' on a US keyboard layout.
    switch (vk) {
        case VK_OEM_4:
            gCornerRadius = std::max(0.0f, gCornerRadius - step);
            return true;
        case VK_OEM_6:
            gCornerRadius = std::min(64.0f, gCornerRadius + step);
            return true;
        default:
            return false;
    }
}

}  // namespace mactw::theme::tweaks
