#pragma once

#include <string>
#include <string_view>

namespace ctt {

[[nodiscard]] std::wstring NormalizeDisplayKey(std::wstring_view deviceName);

}  // namespace ctt
