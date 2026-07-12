#include "win32/ClearTypeSettings.h"
#include "win32/MainWindow.h"
#include "win32/MonitorService.h"
#include "win32/Preferences.h"
#include "win32/Win32Error.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#include <string>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kInstanceMutexName[] = L"Local\\ClearTune.1B6AF97A-28E4-4D72-A505-A89884A6C942";
constexpr wchar_t kWindowTitle[] = L"ClearTune";

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~ScopedHandle() {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    [[nodiscard]] HANDLE Get() const noexcept { return handle_; }

private:
    HANDLE handle_{};
};

class ScopedComInitialization {
public:
    ScopedComInitialization() noexcept
        : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)),
          ownsInitialization_(SUCCEEDED(result_)) {}

    ~ScopedComInitialization() {
        if (ownsInitialization_) {
            CoUninitialize();
        }
    }

    ScopedComInitialization(const ScopedComInitialization&) = delete;
    ScopedComInitialization& operator=(const ScopedComInitialization&) = delete;

    [[nodiscard]] bool IsUsable() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{};
    bool ownsInitialization_{false};
};

int ShowFatalError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"ClearTune", MB_OK | MB_ICONERROR);
    return 1;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    ScopedHandle instanceMutex(CreateMutexW(nullptr, FALSE, kInstanceMutexName));
    if (instanceMutex.Get() == nullptr) {
        return ShowFatalError(ctt::win32::MakeWindowsError(
            L"Unable to create the single-instance guard", GetLastError()));
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existingWindow = FindWindowW(nullptr, kWindowTitle); existingWindow != nullptr) {
            ShowWindow(existingWindow, SW_RESTORE);
            SetForegroundWindow(existingWindow);
        }
        return 0;
    }
    // The manifest already requests PerMonitorV2. This call keeps unpackaged and
    // development builds DPI-aware when a host ignores the manifest.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = static_cast<DWORD>(sizeof(commonControls));
    commonControls.dwICC = ICC_STANDARD_CLASSES;
    if (InitCommonControlsEx(&commonControls) == FALSE) {
        return ShowFatalError(ctt::win32::MakeWindowsError(
            L"Unable to initialize Windows controls", GetLastError()));
    }

    ScopedComInitialization comInitialization;
    if (!comInitialization.IsUsable()) {
        return ShowFatalError(L"Unable to initialize Windows graphics services.");
    }

    auto monitors = ctt::win32::EnumerateActiveMonitors();
    if (monitors.empty()) {
        return ShowFatalError(L"No active monitors were detected.");
    }

    std::vector<std::wstring> displayKeys;
    displayKeys.reserve(monitors.size());
    for (const auto& monitor : monitors) {
        displayKeys.push_back(monitor.displayKey);
    }

    ctt::win32::ClearTypeSettingsSession settings;
    std::wstring error;
    if (!settings.Capture(displayKeys, error)) {
        return ShowFatalError(error);
    }

    ctt::win32::MainWindow window(
        instance,
        std::move(monitors),
        settings,
        ctt::win32::LoadThemeMode());
    if (!window.CreateAndShow(showCommand, error)) {
        return ShowFatalError(error);
    }

    MSG message{};
    int exitCode = 0;
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            exitCode = static_cast<int>(message.wParam);
            break;
        }
        if (result == -1) {
            exitCode = ShowFatalError(ctt::win32::MakeWindowsError(
                L"The Windows message loop failed", GetLastError()));
            break;
        }
        if (IsDialogMessageW(window.Handle(), &message) == FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    return exitCode;
}
