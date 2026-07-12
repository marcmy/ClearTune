#include "win32/MonitorService.h"

#include "core/DisplayKey.h"

#include <algorithm>
#include <utility>

namespace ctt::win32 {
namespace {

std::wstring FindFriendlyName(const wchar_t* adapterName) {
    DISPLAY_DEVICEW monitor{};
    monitor.cb = static_cast<DWORD>(sizeof(monitor));
    for (DWORD index = 0; EnumDisplayDevicesW(adapterName, index, &monitor, 0) != FALSE; ++index) {
        if ((monitor.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0 && monitor.DeviceString[0] != L'\0') {
            return monitor.DeviceString;
        }
        monitor = {};
        monitor.cb = static_cast<DWORD>(sizeof(monitor));
    }

    DISPLAY_DEVICEW adapter{};
    adapter.cb = static_cast<DWORD>(sizeof(adapter));
    if (EnumDisplayDevicesW(adapterName, 0, &adapter, 0) != FALSE && adapter.DeviceString[0] != L'\0') {
        return adapter.DeviceString;
    }
    return adapterName;
}

BOOL CALLBACK CollectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM parameter) {
    auto& result = *reinterpret_cast<std::vector<MonitorDescriptor>*>(parameter);
    MONITORINFOEXW info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    if (GetMonitorInfoW(handle, &info) == FALSE) {
        return TRUE;
    }

    MonitorDescriptor descriptor;
    descriptor.handle = handle;
    descriptor.deviceName = info.szDevice;
    descriptor.displayKey = NormalizeDisplayKey(descriptor.deviceName);
    descriptor.friendlyName = FindFriendlyName(info.szDevice);
    descriptor.bounds = info.rcMonitor;
    descriptor.width = static_cast<unsigned int>(std::max<LONG>(0, info.rcMonitor.right - info.rcMonitor.left));
    descriptor.height = static_cast<unsigned int>(std::max<LONG>(0, info.rcMonitor.bottom - info.rcMonitor.top));
    descriptor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    DEVMODEW mode{};
    mode.dmSize = static_cast<WORD>(sizeof(mode));
    if (EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0) != FALSE) {
        descriptor.width = mode.dmPelsWidth;
        descriptor.height = mode.dmPelsHeight;
        descriptor.portrait =
            (mode.dmFields & DM_DISPLAYORIENTATION) != 0 &&
            (mode.dmDisplayOrientation == DMDO_90 || mode.dmDisplayOrientation == DMDO_270);
    } else {
        descriptor.portrait = descriptor.height > descriptor.width;
    }

    result.push_back(std::move(descriptor));
    return TRUE;
}

}  // namespace

std::vector<MonitorDescriptor> EnumerateActiveMonitors() {
    std::vector<MonitorDescriptor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    std::stable_sort(monitors.begin(), monitors.end(), [](const MonitorDescriptor& left, const MonitorDescriptor& right) {
        if (left.primary != right.primary) {
            return left.primary;
        }
        if (left.bounds.left != right.bounds.left) {
            return left.bounds.left < right.bounds.left;
        }
        return left.bounds.top < right.bounds.top;
    });
    return monitors;
}

}  // namespace ctt::win32
