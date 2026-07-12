#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>

namespace ctt::win32 {

struct MonitorDescriptor {
    HMONITOR handle{};
    std::wstring deviceName;
    std::wstring displayKey;
    std::wstring friendlyName;
    RECT bounds{};
    unsigned int width{};
    unsigned int height{};
    unsigned int nativeWidth{};
    unsigned int nativeHeight{};
    bool primary{};
    bool portrait{};
    bool nativeResolutionKnown{};
    bool atNativeResolution{};
};

[[nodiscard]] std::vector<MonitorDescriptor> EnumerateActiveMonitors();

}  // namespace ctt::win32
