#include "win32/MonitorService.h"

#include "core/DisplayKey.h"
#include "core/MonitorIdentity.h"
#include "core/Resolution.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ctt::win32 {
namespace {

constexpr wchar_t kRegistryMachinePrefix[] = L"\Registry\Machine\";
constexpr wchar_t kDisplayEnumBase[] = L"SYSTEM\CurrentControlSet\Enum\DISPLAY";
constexpr wchar_t kDeviceParameters[] = L"Device Parameters";
constexpr wchar_t kEdidValue[] = L"EDID";
constexpr std::size_t kEdidGammaOffset = 0x17;

std::optional<int> GammaFromEdid(const std::vector<BYTE>& edid) noexcept {
    if (edid.size() <= kEdidGammaOffset || edid[kEdidGammaOffset] == 0xFF) {
        return std::nullopt;
    }
    const int gammaLevel = (static_cast<int>(edid[kEdidGammaOffset]) + 100) * 10;
    if (gammaLevel < 1000 || gammaLevel > 5000) {
        return std::nullopt;
    }
    return gammaLevel;
}

std::optional<int> ReadEdidGamma(HKEY root, const std::wstring& keyPath) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, keyPath.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD size = 0;
    const LSTATUS sizeStatus = RegGetValueW(
        key,
        nullptr,
        kEdidValue,
        RRF_RT_REG_BINARY,
        nullptr,
        nullptr,
        &size);
    if (sizeStatus != ERROR_SUCCESS || size == 0) {
        RegCloseKey(key);
        return std::nullopt;
    }

    std::vector<BYTE> edid(size);
    const LSTATUS readStatus = RegGetValueW(
        key,
        nullptr,
        kEdidValue,
        RRF_RT_REG_BINARY,
        nullptr,
        edid.data(),
        &size);
    RegCloseKey(key);
    if (readStatus != ERROR_SUCCESS) {
        return std::nullopt;
    }
    edid.resize(size);
    return GammaFromEdid(edid);
}

std::optional<std::wstring> MonitorHardwareId(const wchar_t* deviceId) {
    if (deviceId == nullptr || *deviceId == L'\0') {
        return std::nullopt;
    }
    const std::wstring_view id{deviceId};
    const std::size_t firstSeparator = id.find(L'\');
    if (firstSeparator == std::wstring_view::npos) {
        return std::nullopt;
    }
    const std::size_t secondSeparator = id.find(L'\', firstSeparator + 1);
    const std::wstring_view hardwareId = id.substr(
        firstSeparator + 1,
        secondSeparator == std::wstring_view::npos
            ? std::wstring_view::npos
            : secondSeparator - firstSeparator - 1);
    if (hardwareId.empty()) {
        return std::nullopt;
    }
    return std::wstring{hardwareId};
}

std::optional<int> ReadMonitorGamma(const DISPLAY_DEVICEW& monitor) {
    if (monitor.DeviceKey[0] != L'\0') {
        std::wstring path{monitor.DeviceKey};
        if (path.starts_with(kRegistryMachinePrefix)) {
            path.erase(0, std::size(kRegistryMachinePrefix) - 1);
        }
        if (const auto direct = ReadEdidGamma(HKEY_LOCAL_MACHINE, path + L"\" + kDeviceParameters)) {
            return direct;
        }
        if (const auto direct = ReadEdidGamma(HKEY_LOCAL_MACHINE, path)) {
            return direct;
        }
    }

    const auto hardwareId = MonitorHardwareId(monitor.DeviceID);
    if (!hardwareId) {
        return std::nullopt;
    }
    const std::wstring hardwarePath = std::wstring{kDisplayEnumBase} + L"\" + *hardwareId;
    HKEY hardwareKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, hardwarePath.c_str(), 0, KEY_ENUMERATE_SUB_KEYS, &hardwareKey) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::optional<int> result;
    for (DWORD index = 0; !result; ++index) {
        std::array<wchar_t, 256> instanceName{};
        DWORD nameLength = static_cast<DWORD>(instanceName.size());
        const LSTATUS status = RegEnumKeyExW(
            hardwareKey,
            index,
            instanceName.data(),
            &nameLength,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (status != ERROR_SUCCESS) {
            continue;
        }
        const std::wstring parametersPath =
            hardwarePath + L"\" + std::wstring{instanceName.data(), nameLength} + L"\" + kDeviceParameters;
        result = ReadEdidGamma(HKEY_LOCAL_MACHINE, parametersPath);
    }
    RegCloseKey(hardwareKey);
    return result;
}

void PopulateMonitorIdentity(const wchar_t* adapterName, MonitorDescriptor& descriptor) {
    DISPLAY_DEVICEW monitor{};
    monitor.cb = static_cast<DWORD>(sizeof(monitor));
    for (DWORD index = 0; EnumDisplayDevicesW(adapterName, index, &monitor, 0) != FALSE; ++index) {
        if ((monitor.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0) {
            descriptor.friendlyName = ChooseMonitorFriendlyName(
                monitor.DeviceString,
                L"",
                adapterName);
            if (const auto gamma = ReadMonitorGamma(monitor)) {
                descriptor.gammaLevel = *gamma;
                descriptor.gammaKnown = true;
            }
            return;
        }
        monitor = {};
        monitor.cb = static_cast<DWORD>(sizeof(monitor));
    }

    DISPLAY_DEVICEW adapter{};
    adapter.cb = static_cast<DWORD>(sizeof(adapter));
    if (EnumDisplayDevicesW(adapterName, 0, &adapter, 0) != FALSE) {
        descriptor.friendlyName = ChooseMonitorFriendlyName(
            adapter.DeviceString,
            L"",
            adapterName);
    } else {
        descriptor.friendlyName = adapterName;
    }
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
    descriptor.bounds = info.rcMonitor;
    descriptor.width = static_cast<unsigned int>(std::max<LONG>(0, info.rcMonitor.right - info.rcMonitor.left));
    descriptor.height = static_cast<unsigned int>(std::max<LONG>(0, info.rcMonitor.bottom - info.rcMonitor.top));
    descriptor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    PopulateMonitorIdentity(info.szDevice, descriptor);

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

void PopulatePreferredModes(std::vector<MonitorDescriptor>& monitors) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
            return;
        }

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const LONG queryStatus = QueryDisplayConfig(
            QDC_ONLY_ACTIVE_PATHS,
            &pathCount,
            paths.data(),
            &modeCount,
            modes.data(),
            nullptr);
        if (queryStatus == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        if (queryStatus != ERROR_SUCCESS) {
            return;
        }

        paths.resize(pathCount);
        for (const auto& path : paths) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
            sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            sourceName.header.size = static_cast<UINT32>(sizeof(sourceName));
            sourceName.header.adapterId = path.sourceInfo.adapterId;
            sourceName.header.id = path.sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS) {
                continue;
            }

            const auto monitor = std::find_if(monitors.begin(), monitors.end(), [&](const MonitorDescriptor& candidate) {
                return candidate.deviceName == sourceName.viewGdiDeviceName;
            });
            if (monitor == monitors.end()) {
                continue;
            }

            DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
            targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            targetName.header.size = static_cast<UINT32>(sizeof(targetName));
            targetName.header.adapterId = path.targetInfo.adapterId;
            targetName.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS) {
                monitor->friendlyName = ChooseMonitorFriendlyName(
                    monitor->friendlyName,
                    targetName.monitorFriendlyDeviceName,
                    monitor->deviceName);
            }

            DISPLAYCONFIG_TARGET_PREFERRED_MODE preferredMode{};
            preferredMode.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
            preferredMode.header.size = static_cast<UINT32>(sizeof(preferredMode));
            preferredMode.header.adapterId = path.targetInfo.adapterId;
            preferredMode.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&preferredMode.header) != ERROR_SUCCESS ||
                preferredMode.width == 0 || preferredMode.height == 0) {
                continue;
            }

            monitor->nativeWidth = preferredMode.width;
            monitor->nativeHeight = preferredMode.height;
            monitor->nativeResolutionKnown = true;
            monitor->atNativeResolution = MeetsOrExceedsPreferredResolution(
                monitor->width,
                monitor->height,
                monitor->nativeWidth,
                monitor->nativeHeight,
                monitor->portrait);
        }
        return;
    }
}

}  // namespace

std::vector<MonitorDescriptor> EnumerateActiveMonitors() {
    std::vector<MonitorDescriptor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    PopulatePreferredModes(monitors);
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
