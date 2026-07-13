#include "core/DisplayKey.h"

namespace ctt {

std::wstring NormalizeDisplayKey(const std::wstring_view deviceName) {
    constexpr std::wstring_view prefix = L"\\\\.\\";
    if (deviceName.starts_with(prefix)) {
        return std::wstring{deviceName.substr(prefix.size())};
    }
    return std::wstring{deviceName};
}

}  // namespace ctt
