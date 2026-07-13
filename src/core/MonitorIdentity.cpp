#include "core/MonitorIdentity.h"

#include <algorithm>
#include <cwctype>

namespace ctt {
namespace {

std::wstring Lowercase(const std::wstring_view value) {
    std::wstring lowered{value};
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return lowered;
}

}  // namespace

bool IsGenericMonitorName(const std::wstring_view name) noexcept {
    if (name.empty()) {
        return true;
    }
    const std::wstring lowered = Lowercase(name);
    return lowered == L"generic pnp monitor" ||
        lowered == L"generic non-pnp monitor" ||
        lowered == L"pnp monitor" ||
        lowered == L"default monitor";
}

std::wstring ChooseMonitorFriendlyName(
    const std::wstring_view legacyName,
    const std::wstring_view displayConfigName,
    const std::wstring_view fallbackName) {
    if (!IsGenericMonitorName(displayConfigName)) {
        return std::wstring{displayConfigName};
    }
    if (!legacyName.empty() && !IsGenericMonitorName(legacyName)) {
        return std::wstring{legacyName};
    }
    if (!displayConfigName.empty()) {
        return std::wstring{displayConfigName};
    }
    if (!legacyName.empty()) {
        return std::wstring{legacyName};
    }
    return std::wstring{fallbackName};
}

}  // namespace ctt
