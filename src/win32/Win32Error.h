#pragma once

#include <string>
#include <string_view>

namespace ctt::win32 {

[[nodiscard]] std::wstring FormatWindowsError(unsigned long errorCode);
[[nodiscard]] std::wstring MakeWindowsError(std::wstring_view operation, unsigned long errorCode);

}  // namespace ctt::win32
