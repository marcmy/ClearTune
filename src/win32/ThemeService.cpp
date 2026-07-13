#include "win32/ThemeService.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include <iterator>

namespace ctt::win32 {
namespace {
constexpr wchar_t kPersonalizeKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightThemeValue[] = L"AppsUseLightTheme";
constexpr DWORD kImmersiveDarkMode = 20;
constexpr DWORD kImmersiveDarkModeBefore20H1 = 19;

BOOL CALLBACK ThemeChildWindow(HWND child, LPARAM parameter) {
    const bool dark = parameter != 0;

    wchar_t className[32]{};
    GetClassNameW(child, className, static_cast<int>(std::size(className)));
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE));
    const DWORD buttonType = style & BS_TYPEMASK;
    const bool radioButton =
        lstrcmpiW(className, L"Button") == 0 &&
        (buttonType == BS_RADIOBUTTON || buttonType == BS_AUTORADIOBUTTON);

    // The undocumented dark Explorer theme paints radio-button labels black on
    // some Windows 11 builds. Leaving those controls unthemed lets the parent
    // WM_CTLCOLORBTN handler provide the correct foreground and background.
    if (dark && radioButton) {
        SetWindowTheme(child, L"", L"");
    } else {
        SetWindowTheme(child, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    return TRUE;
}
}

bool WindowsAppsUseLightTheme() noexcept {
    DWORD value = 1;
    DWORD size = static_cast<DWORD>(sizeof(value));
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kPersonalizeKey,
        kAppsUseLightThemeValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status != ERROR_SUCCESS || value != 0;
}

bool IsDarkTheme(const ThemeMode mode) noexcept {
    return ResolveDarkTheme(mode, WindowsAppsUseLightTheme());
}

void ApplyWindowTheme(HWND window, const ThemeMode mode) noexcept {
    if (window == nullptr) {
        return;
    }
    const bool dark = IsDarkTheme(mode);
    const BOOL darkValue = dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(window, kImmersiveDarkMode, &darkValue, static_cast<DWORD>(sizeof(darkValue))))) {
        DwmSetWindowAttribute(window, kImmersiveDarkModeBefore20H1, &darkValue, static_cast<DWORD>(sizeof(darkValue)));
    }
    SetWindowTheme(window, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    EnumChildWindows(window, ThemeChildWindow, dark ? 1 : 0);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

}  // namespace ctt::win32
