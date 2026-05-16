#include "theme/AppTheme.h"

namespace vrtx::theme {

const Palette& ActivePalette() {
    // MVP ships dark only. When the settings UI arrives this will switch on a
    // user preference and react to WM_SETTINGCHANGE for system theme.
    return kDarkPalette;
}

}  // namespace vrtx::theme
