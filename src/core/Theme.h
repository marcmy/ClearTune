#pragma once

namespace ctt {

enum class ThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
};

[[nodiscard]] ThemeMode ThemeModeFromStoredValue(int value) noexcept;
[[nodiscard]] int ThemeModeToStoredValue(ThemeMode mode) noexcept;
[[nodiscard]] bool ResolveDarkTheme(ThemeMode mode, bool windowsAppsUseLightTheme) noexcept;
[[nodiscard]] bool ResolvePreviewDark(
    ThemeMode mode,
    bool windowsAppsUseLightTheme,
    bool comparePolarity) noexcept;

}  // namespace ctt
