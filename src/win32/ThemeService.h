#pragma once

#include "core/Theme.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ctt::win32 {

[[nodiscard]] bool WindowsAppsUseLightTheme() noexcept;
[[nodiscard]] bool IsDarkTheme(ThemeMode mode) noexcept;
void ApplyWindowTheme(HWND window, ThemeMode mode) noexcept;

}  // namespace ctt::win32
