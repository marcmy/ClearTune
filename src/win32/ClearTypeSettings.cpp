#include "win32/ClearTypeSettings.h"

#include "core/Conversions.h"
#include "win32/Win32Error.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace ctt::win32 {
namespace {
constexpr wchar_t kAvalonBaseKey[] = L"Software\\Microsoft\\Avalon.Graphics";
constexpr wchar_t kGammaLevel[] = L"GammaLevel";
constexpr wchar_t kPixelStructure[] = L"PixelStructure";
constexpr wchar_t kClearTypeLevel[] = L"ClearTypeLevel";
constexpr wchar_t kTextContrastLevel[] = L"TextContrastLevel";
constexpr wchar_t kEnhancedContrastLevel[] = L"EnhancedContrastLevel";
constexpr wchar_t kGrayscaleEnhancedContrastLevel[] = L"GrayscaleEnhancedContrastLevel";
constexpr UINT kPersistentApplyFlags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;

std::wstring DisplayPath(const std::wstring_view displayKey) {
    std::wstring path{kAvalonBaseKey};
    path.push_back(L'\\');
    path.append(displayKey);
    return path;
}

OptionalDword ReadDword(HKEY key, const wchar_t* name) noexcept {
    OptionalDword result;
    DWORD value = 0;
    DWORD size = static_cast<DWORD>(sizeof(value));
    const LSTATUS status = RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status == ERROR_SUCCESS) {
        result.present = true;
        result.value = value;
    }
    return result;
}

bool WriteDword(HKEY key, const wchar_t* name, const DWORD value, std::wstring& error) {
    const LSTATUS status = RegSetValueExW(
        key,
        name,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        static_cast<DWORD>(sizeof(value)));
    if (status != ERROR_SUCCESS) {
        error = MakeWindowsError(std::wstring{L"Unable to write "} + name, static_cast<unsigned long>(status));
        return false;
    }
    return true;
}

bool RestoreDword(HKEY key, const wchar_t* name, const OptionalDword original, std::wstring& error) {
    if (original.present) {
        return WriteDword(key, name, original.value, error);
    }
    const LSTATUS status = RegDeleteValueW(key, name);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
        error = MakeWindowsError(std::wstring{L"Unable to remove "} + name, static_cast<unsigned long>(status));
        return false;
    }
    return true;
}

ClearTypeProfile ProfileFromSnapshot(const PerDisplaySnapshot& snapshot) {
    ClearTypeProfile profile;
    if (snapshot.pixelStructure.present) {
        profile.pixelStructure = static_cast<int>(snapshot.pixelStructure.value);
    }
    if (snapshot.gammaLevel.present) {
        profile.gammaLevel = static_cast<int>(snapshot.gammaLevel.value);
    }
    if (snapshot.clearTypeLevel.present) {
        profile.clearTypeLevel = static_cast<int>(snapshot.clearTypeLevel.value);
    }
    if (snapshot.textContrastLevel.present) {
        profile.textContrastLevel = static_cast<int>(snapshot.textContrastLevel.value);
    }
    if (snapshot.enhancedContrastLevel.present) {
        profile.enhancedContrastLevel = static_cast<int>(snapshot.enhancedContrastLevel.value);
    }
    if (snapshot.grayscaleEnhancedContrastLevel.present) {
        profile.grayscaleEnhancedContrastLevel = static_cast<int>(snapshot.grayscaleEnhancedContrastLevel.value);
    }
    return profile;
}

PVOID SpiValue(const UINT value) noexcept {
    return reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(value));
}

bool SetSpi(
    const UINT action,
    const UINT parameter,
    PVOID value,
    const UINT flags,
    const std::wstring_view operation,
    std::wstring& error) {
    if (SystemParametersInfoW(action, parameter, value, flags) == FALSE) {
        error = MakeWindowsError(operation, GetLastError());
        return false;
    }
    return true;
}

void RedrawAllWindows() noexcept {
    RedrawWindow(nullptr, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

}  // namespace

bool ClearTypeSettingsSession::Capture(std::span<const std::wstring> displayKeys, std::wstring& error) {
    captured_ = false;
    previewActive_ = false;
    displays_.clear();
    initialProfiles_.clear();

    if (!CaptureGlobal(error)) {
        return false;
    }

    displays_.reserve(displayKeys.size());
    initialProfiles_.reserve(displayKeys.size());
    for (const auto& displayKey : displayKeys) {
        PerDisplaySnapshot snapshot;
        snapshot.displayKey = displayKey;
        const std::wstring path = DisplayPath(displayKey);
        HKEY key = nullptr;
        const LSTATUS openStatus = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key);
        if (openStatus == ERROR_SUCCESS) {
            snapshot.keyExisted = true;
            snapshot.gammaLevel = ReadDword(key, kGammaLevel);
            snapshot.pixelStructure = ReadDword(key, kPixelStructure);
            snapshot.clearTypeLevel = ReadDword(key, kClearTypeLevel);
            snapshot.textContrastLevel = ReadDword(key, kTextContrastLevel);
            snapshot.enhancedContrastLevel = ReadDword(key, kEnhancedContrastLevel);
            snapshot.grayscaleEnhancedContrastLevel = ReadDword(key, kGrayscaleEnhancedContrastLevel);
            RegCloseKey(key);
        } else if (openStatus != ERROR_FILE_NOT_FOUND) {
            error = MakeWindowsError(
                L"Unable to read ClearType settings for " + displayKey,
                static_cast<unsigned long>(openStatus));
            return false;
        }

        ClearTypeProfile profile = ProfileFromSnapshot(snapshot);
        if (!snapshot.gammaLevel.present) {
            profile.gammaLevel = static_cast<int>(std::clamp(global_.contrast, 1000U, 2200U));
        }
        if (!snapshot.pixelStructure.present) {
            profile.pixelStructure = global_.orientation == FE_FONTSMOOTHINGORIENTATIONBGR ? 2 : 1;
        }
        initialProfiles_.push_back(profile);
        displays_.push_back(std::move(snapshot));
    }

    captured_ = true;
    return true;
}

bool ClearTypeSettingsSession::CaptureGlobal(std::wstring& error) {
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &global_.enabled, 0) == FALSE) {
        error = MakeWindowsError(L"Unable to read font smoothing state", GetLastError());
        return false;
    }
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &global_.type, 0) == FALSE) {
        error = MakeWindowsError(L"Unable to read font smoothing type", GetLastError());
        return false;
    }
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &global_.contrast, 0) == FALSE) {
        error = MakeWindowsError(L"Unable to read font smoothing contrast", GetLastError());
        return false;
    }
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &global_.orientation, 0) == FALSE) {
        error = MakeWindowsError(L"Unable to read font smoothing orientation", GetLastError());
        return false;
    }
    return true;
}

bool ClearTypeSettingsSession::WriteDisplayProfile(const ApplyTarget& target, std::wstring& error) {
    const std::wstring path = DisplayPath(target.displayKey);
    HKEY key = nullptr;
    const LSTATUS createStatus = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        path.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        error = MakeWindowsError(
            L"Unable to open ClearType settings for " + target.displayKey,
            static_cast<unsigned long>(createStatus));
        return false;
    }

    const std::array<std::pair<const wchar_t*, DWORD>, 6> values{{
        {kGammaLevel, static_cast<DWORD>(std::clamp(target.profile.gammaLevel, 1000, 5000))},
        {kPixelStructure, static_cast<DWORD>(std::clamp(target.profile.pixelStructure, 0, 2))},
        {kClearTypeLevel, static_cast<DWORD>(std::clamp(target.profile.clearTypeLevel, 0, 100))},
        {kTextContrastLevel, static_cast<DWORD>(std::clamp(target.profile.textContrastLevel, 0, 6))},
        {kEnhancedContrastLevel, static_cast<DWORD>(std::clamp(target.profile.enhancedContrastLevel, 0, 1000))},
        {kGrayscaleEnhancedContrastLevel,
         static_cast<DWORD>(std::clamp(target.profile.grayscaleEnhancedContrastLevel, 0, 1000))},
    }};

    bool success = true;
    for (const auto& [name, value] : values) {
        if (!WriteDword(key, name, value, error)) {
            success = false;
            break;
        }
    }
    RegCloseKey(key);
    return success;
}

bool ClearTypeSettingsSession::ApplyGlobal(
    const ClearTypeProfile& primaryProfile,
    const bool enableClearType,
    const int globalContrast,
    const UINT flags,
    std::wstring& error) {
    if (!SetSpi(
            SPI_SETFONTSMOOTHING,
            enableClearType ? TRUE : FALSE,
            nullptr,
            flags,
            L"Unable to set font smoothing state",
            error)) {
        return false;
    }
    const UINT type = enableClearType ? FE_FONTSMOOTHINGCLEARTYPE : FE_FONTSMOOTHINGSTANDARD;
    if (!SetSpi(
            SPI_SETFONTSMOOTHINGTYPE,
            0,
            SpiValue(type),
            flags,
            L"Unable to set font smoothing type",
            error)) {
        return false;
    }
    if (!enableClearType) {
        return true;
    }
    if (!SetSpi(
            SPI_SETFONTSMOOTHINGCONTRAST,
            0,
            SpiValue(static_cast<UINT>(std::clamp(globalContrast, 1000, 2200))),
            flags,
            L"Unable to set font smoothing contrast",
            error)) {
        return false;
    }
    if (!SetSpi(
            SPI_SETFONTSMOOTHINGORIENTATION,
            0,
            SpiValue(ToSpiOrientation(primaryProfile)),
            flags,
            L"Unable to set font smoothing orientation",
            error)) {
        return false;
    }
    return true;
}

bool ClearTypeSettingsSession::Preview(
    const ClearTypeProfile& profile,
    const bool enableClearType,
    const int globalContrast,
    std::wstring& error) {
    if (!captured_) {
        error = L"ClearType settings were not captured before Preview.";
        return false;
    }
    if (!ApplyGlobal(profile, enableClearType, globalContrast, 0, error)) {
        return false;
    }
    previewActive_ = true;
    RedrawAllWindows();
    return true;
}

bool ClearTypeSettingsSession::RestorePreview(std::wstring& error) {
    if (!captured_ || !previewActive_) {
        return true;
    }
    const bool success = RestoreGlobal(0, error);
    previewActive_ = false;
    RedrawAllWindows();
    return success;
}

bool ClearTypeSettingsSession::Apply(
    const std::span<const ApplyTarget> targets,
    const bool enableClearType,
    const int globalContrast,
    std::wstring& error) {
    if (!captured_ || targets.empty()) {
        error = L"ClearType settings were not captured before Apply.";
        return false;
    }

    if (enableClearType) {
        for (const auto& target : targets) {
            if (!WriteDisplayProfile(target, error)) {
                std::wstring rollbackError;
                const bool restored = Restore(rollbackError);
                if (!restored && rollbackError.empty()) {
                    rollbackError = L"The original ClearType settings could not be fully restored.";
                }
                if (!rollbackError.empty()) {
                    error.append(L"\nRollback also reported: ").append(rollbackError);
                }
                return false;
            }
        }
    }

    const auto primary = std::find_if(
        targets.begin(), targets.end(), [](const ApplyTarget& target) { return target.primary; });
    const ClearTypeProfile& globalProfile = (primary != targets.end() ? primary : targets.begin())->profile;
    if (!ApplyGlobal(globalProfile, enableClearType, globalContrast, kPersistentApplyFlags, error)) {
        std::wstring rollbackError;
        const bool restored = Restore(rollbackError);
        if (!restored && rollbackError.empty()) {
            rollbackError = L"The original ClearType settings could not be fully restored.";
        }
        if (!rollbackError.empty()) {
            error.append(L"\nRollback also reported: ").append(rollbackError);
        }
        return false;
    }

    previewActive_ = false;
    SendMessageTimeoutW(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        reinterpret_cast<LPARAM>(L"WindowMetrics"),
        SMTO_ABORTIFHUNG,
        2000,
        nullptr);
    return true;
}

bool ClearTypeSettingsSession::RestoreDisplay(const PerDisplaySnapshot& snapshot, std::wstring& error) {
    const std::wstring path = DisplayPath(snapshot.displayKey);
    HKEY key = nullptr;
    const LSTATUS createStatus = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        path.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        error = MakeWindowsError(
            L"Unable to restore ClearType settings for " + snapshot.displayKey,
            static_cast<unsigned long>(createStatus));
        return false;
    }

    bool success = true;
    std::wstring firstError;
    const auto restoreValue = [&](const wchar_t* name, const OptionalDword value) {
        std::wstring valueError;
        if (!RestoreDword(key, name, value, valueError)) {
            success = false;
            if (firstError.empty()) {
                firstError = std::move(valueError);
            }
        }
    };
    restoreValue(kGammaLevel, snapshot.gammaLevel);
    restoreValue(kPixelStructure, snapshot.pixelStructure);
    restoreValue(kClearTypeLevel, snapshot.clearTypeLevel);
    restoreValue(kTextContrastLevel, snapshot.textContrastLevel);
    restoreValue(kEnhancedContrastLevel, snapshot.enhancedContrastLevel);
    restoreValue(kGrayscaleEnhancedContrastLevel, snapshot.grayscaleEnhancedContrastLevel);
    if (!firstError.empty()) {
        error = std::move(firstError);
    }
    RegCloseKey(key);

    if (success && !snapshot.keyExisted) {
        const LSTATUS deleteStatus = RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
        if (deleteStatus != ERROR_SUCCESS && deleteStatus != ERROR_FILE_NOT_FOUND) {
            error = MakeWindowsError(
                L"Unable to remove temporary ClearType display key",
                static_cast<unsigned long>(deleteStatus));
            success = false;
        }
    }
    return success;
}

bool ClearTypeSettingsSession::RestoreGlobal(const UINT flags, std::wstring& error) {
    bool success = true;
    std::wstring firstError;
    const auto restore = [&](const UINT action, const UINT parameter, PVOID value, const wchar_t* operation) {
        std::wstring valueError;
        if (!SetSpi(action, parameter, value, flags, operation, valueError)) {
            success = false;
            if (firstError.empty()) {
                firstError = std::move(valueError);
            }
        }
    };
    restore(
        SPI_SETFONTSMOOTHING,
        global_.enabled ? TRUE : FALSE,
        nullptr,
        L"Unable to restore font smoothing state");
    restore(
        SPI_SETFONTSMOOTHINGTYPE,
        0,
        SpiValue(global_.type),
        L"Unable to restore font smoothing type");
    restore(
        SPI_SETFONTSMOOTHINGCONTRAST,
        0,
        SpiValue(global_.contrast),
        L"Unable to restore font smoothing contrast");
    restore(
        SPI_SETFONTSMOOTHINGORIENTATION,
        0,
        SpiValue(global_.orientation),
        L"Unable to restore font smoothing orientation");
    if (!firstError.empty()) {
        error = std::move(firstError);
    }
    return success;
}

bool ClearTypeSettingsSession::Restore(std::wstring& error) {
    if (!captured_) {
        return true;
    }
    bool success = true;
    for (const auto& display : displays_) {
        std::wstring displayError;
        if (!RestoreDisplay(display, displayError)) {
            success = false;
            if (error.empty()) {
                error = std::move(displayError);
            }
        }
    }
    std::wstring globalError;
    if (!RestoreGlobal(kPersistentApplyFlags, globalError)) {
        success = false;
        if (error.empty()) {
            error = std::move(globalError);
        }
    }
    previewActive_ = false;
    return success;
}

const std::vector<ClearTypeProfile>& ClearTypeSettingsSession::InitialProfiles() const noexcept {
    return initialProfiles_;
}

bool ClearTypeSettingsSession::InitialClearTypeEnabled() const noexcept {
    return global_.enabled != FALSE && global_.type == FE_FONTSMOOTHINGCLEARTYPE;
}

int ClearTypeSettingsSession::InitialGlobalContrast() const noexcept {
    return static_cast<int>(std::clamp(global_.contrast, 1000U, 2200U));
}

}  // namespace ctt::win32
