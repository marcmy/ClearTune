#pragma once

#include <string>
#include <string_view>

namespace ctt {

[[nodiscard]] bool IsGenericMonitorName(std::wstring_view name) noexcept;

[[nodiscard]] std::wstring ChooseMonitorFriendlyName(
    std::wstring_view legacyName,
    std::wstring_view displayConfigName,
    std::wstring_view fallbackName);

}  // namespace ctt
