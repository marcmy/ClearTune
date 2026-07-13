#include "win32/Preferences.h"

#include "win32/Win32Error.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ctt::win32 {
namespace {
constexpr wchar_t kPreferencesKey[] = L"Software\\ClearTune";
constexpr wchar_t kThemeModeValue[] = L"ThemeMode";
}

ThemeMode LoadThemeMode() noexcept {
    DWORD value = 0;
    DWORD size = static_cast<DWORD>(sizeof(value));
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kPreferencesKey,
        kThemeModeValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS) {
        return ThemeMode::System;
    }
    return ThemeModeFromStoredValue(static_cast<int>(value));
}

bool SaveThemeMode(const ThemeMode mode, std::wstring& error) {
    HKEY key = nullptr;
    const LSTATUS createStatus = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kPreferencesKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        error = MakeWindowsError(L"Unable to open application preferences", static_cast<unsigned long>(createStatus));
        return false;
    }

    const DWORD value = static_cast<DWORD>(ThemeModeToStoredValue(mode));
    const LSTATUS writeStatus = RegSetValueExW(
        key,
        kThemeModeValue,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        static_cast<DWORD>(sizeof(value)));
    RegCloseKey(key);
    if (writeStatus != ERROR_SUCCESS) {
        error = MakeWindowsError(L"Unable to save theme preference", static_cast<unsigned long>(writeStatus));
        return false;
    }
    return true;
}

}  // namespace ctt::win32
