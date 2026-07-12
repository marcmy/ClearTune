#include "core/Theme.h"

namespace ctt {

ThemeMode ThemeModeFromStoredValue(const int value) noexcept {
    switch (value) {
        case 1:
            return ThemeMode::Light;
        case 2:
            return ThemeMode::Dark;
        default:
            return ThemeMode::System;
    }
}

int ThemeModeToStoredValue(const ThemeMode mode) noexcept {
    return static_cast<int>(mode);
}

bool ResolveDarkTheme(const ThemeMode mode, const bool windowsAppsUseLightTheme) noexcept {
    switch (mode) {
        case ThemeMode::Light:
            return false;
        case ThemeMode::Dark:
            return true;
        case ThemeMode::System:
        default:
            return !windowsAppsUseLightTheme;
    }
}

}  // namespace ctt
