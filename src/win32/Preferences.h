#pragma once

#include "core/Theme.h"

#include <string>

namespace ctt::win32 {

[[nodiscard]] ThemeMode LoadThemeMode() noexcept;
[[nodiscard]] bool SaveThemeMode(ThemeMode mode, std::wstring& error);

}  // namespace ctt::win32
