#include "win32/MainWindow.h"

#include "core/Candidates.h"
#include "win32/ThemeService.h"
#include "win32/Win32Error.h"

#include <commctrl.h>

#include <algorithm>
#include <utility>

namespace ctt::win32 {
namespace {
constexpr wchar_t kWindowClass[] = L"ClearTune.MainWindow";
constexpr wchar_t kWindowTitle[] = L"ClearTune";
constexpr int kApplicationIconId = 101;
constexpr int kThemeComboId = 100;
constexpr int kClearTypeCheckId = 101;
constexpr int kTuneAllRadioId = 102;
constexpr int kTuneOneRadioId = 103;
constexpr int kMonitorMapId = 104;
constexpr int kCompareButtonId = 105;
constexpr int kBackButtonId = 200;
constexpr int kNextButtonId = IDOK;
constexpr int kCancelButtonId = IDCANCEL;
constexpr int kSampleBaseId = 1000;
constexpr int kFinishLightPreviewId = 1100;
constexpr int kFinishDarkPreviewId = 1101;
constexpr wchar_t kSampleText[] =
    L"The Quick Brown Fox Jumps Over the Lazy Dog. Lorem ipsum dolor sit amet, consectetuer adipiscing elit. "
    L"Mauris ornare odio vel risus. Maecenas elit metus, pellentesque quis, pretium.\n\n"
    L"Clear text should look even, calm, and easy to read.";

constexpr COLORREF LightBackground() noexcept { return RGB(249, 249, 249); }
constexpr COLORREF LightText() noexcept { return RGB(24, 24, 24); }
constexpr COLORREF DarkBackground() noexcept { return RGB(32, 32, 32); }
constexpr COLORREF DarkText() noexcept { return RGB(242, 242, 242); }

}  // namespace

MainWindow::MainWindow(
    HINSTANCE instance,
    std::vector<MonitorDescriptor> monitors,
    ClearTypeSettingsSession& settings,
    const ThemeMode themeMode)
    : instance_(instance),
      monitors_(std::move(monitors)),
      settings_(settings),
      model_(settings.InitialProfiles(), settings.InitialGlobalContrast()),
      themeMode_(themeMode),
      clearTypeEnabled_(settings.InitialClearTypeEnabled()) {
    activeMonitorIndices_.reserve(monitors_.size());
    for (std::size_t index = 0; index < monitors_.size(); ++index) {
        activeMonitorIndices_.push_back(index);
    }
}

MainWindow::~MainWindow() {
    if (backgroundBrush_ != nullptr) {
        DeleteObject(backgroundBrush_);
    }
    DestroyFonts();
}

bool MainWindow::RegisterWindowClass(std::wstring& error) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = static_cast<UINT>(sizeof(windowClass));
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIcon = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(kApplicationIconId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR));
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(kApplicationIconId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));

    if (RegisterClassExW(&windowClass) == 0) {
        const DWORD lastError = GetLastError();
        if (lastError != ERROR_CLASS_ALREADY_EXISTS) {
            error = MakeWindowsError(L"Unable to register the application window", lastError);
            return false;
        }
    }
    return true;
}

bool MainWindow::CreateAndShow(const int showCommand, std::wstring& error) {
    if (!RegisterWindowClass(error)) {
        return false;
    }
    if (!renderer_.Initialize(error)) {
        return false;
    }

    const UINT initialDpi = GetDpiForSystem();
    RECT desired{0, 0, MulDiv(840, static_cast<int>(initialDpi), 96), MulDiv(620, static_cast<int>(initialDpi), 96)};
    AdjustWindowRectExForDpi(
        &desired,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE,
        WS_EX_CONTROLPARENT,
        initialDpi);
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    window_ = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        std::max(0, (screenWidth - width) / 2),
        std::max(0, (screenHeight - height) / 2),
        width,
        height,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        error = MakeWindowsError(L"Unable to create the application window", GetLastError());
        return false;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    return true;
}

HWND MainWindow::Handle() const noexcept { return window_; }

LRESULT CALLBACK MainWindow::WindowProcedure(HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    return self != nullptr
        ? self->HandleMessage(message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            dpi_ = GetDpiForWindow(window_);
            std::wstring error;
            RecreateBackgroundBrush();
            if (!CreateControls(error)) {
                MessageBoxW(window_, error.c_str(), L"ClearTune", MB_OK | MB_ICONERROR);
                return -1;
            }
            ApplyTheme();
            RefreshPage();
            return 0;
        }
        case WM_SIZE:
            Layout();
            return 0;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wParam);
            const auto* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(
                window_,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            CreateFonts();
            Layout();
            InvalidateRect(monitorMap_, nullptr, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            const int identifier = LOWORD(wParam);
            const int notification = HIWORD(wParam);
            if (identifier == kThemeComboId && notification == CBN_SELCHANGE) {
                ThemeSelectionChanged();
                return 0;
            }
            if (identifier == kCompareButtonId && notification == BN_CLICKED) {
                ComparePolarity();
                return 0;
            }
            if (identifier == kClearTypeCheckId && notification == BN_CLICKED) {
                clearTypeEnabled_ = SendMessageW(clearTypeCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                UpdateWelcomeControls();
                return 0;
            }
            if ((identifier == kTuneAllRadioId || identifier == kTuneOneRadioId) && notification == BN_CLICKED) {
                MonitorSelectionChanged();
                return 0;
            }
            if (identifier == kBackButtonId && notification == BN_CLICKED) {
                NavigateBack();
                return 0;
            }
            if (identifier == kNextButtonId && notification == BN_CLICKED) {
                NavigateNext();
                return 0;
            }
            if (identifier == kCancelButtonId && notification == BN_CLICKED) {
                CancelAndClose();
                return 0;
            }
            if (identifier >= kSampleBaseId &&
                identifier < kSampleBaseId + static_cast<int>(sampleButtons_.size()) &&
                notification == BN_CLICKED) {
                SelectSample(static_cast<std::size_t>(identifier - kSampleBaseId));
                return 0;
            }
            break;
        }
        case WM_DRAWITEM: {
            const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw->CtlID == static_cast<UINT>(kMonitorMapId)) {
                DrawMonitorMap(*draw);
                return TRUE;
            }
            const UINT sampleBaseId = static_cast<UINT>(kSampleBaseId);
            if (draw->CtlID >= sampleBaseId &&
                draw->CtlID < sampleBaseId + static_cast<UINT>(sampleButtons_.size())) {
                const std::size_t visualIndex = static_cast<std::size_t>(draw->CtlID - sampleBaseId);
                if (model_.IsSamplePage() && visualIndex < CandidateCount(model_.CurrentStage())) {
                    const std::size_t candidateIndex = CandidateIndexForPolarity(
                        model_.CurrentStage(), visualIndex, IsDark());
                    const ClearTypeProfile profile = model_.CandidateRenderingProfile(candidateIndex);
                    std::wstring error;
                    const bool selected = candidateIndex == model_.SelectedCandidateIndex();
                    const bool focused = (draw->itemState & ODS_FOCUS) != 0;
                    if (!renderer_.DrawSample(
                            draw->hDC,
                            draw->rcItem,
                            profile,
                            model_.CurrentStage(),
                            dpi_,
                            IsDark(),
                            selected,
                            focused,
                            kSampleText,
                            error)) {
                        SetBkColor(draw->hDC, IsDark() ? DarkBackground() : LightBackground());
                        SetTextColor(draw->hDC, IsDark() ? DarkText() : LightText());
                        FillRect(draw->hDC, &draw->rcItem, backgroundBrush_);
                        DrawTextW(
                            draw->hDC,
                            L"Unable to render sample",
                            -1,
                            const_cast<RECT*>(&draw->rcItem),
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                    return TRUE;
                }
            }
            if (draw->CtlID == static_cast<UINT>(kFinishLightPreviewId) ||
                draw->CtlID == static_cast<UINT>(kFinishDarkPreviewId)) {
                std::wstring error;
                const bool dark = draw->CtlID == static_cast<UINT>(kFinishDarkPreviewId);
                const bool focused = (draw->itemState & ODS_FOCUS) != 0;
                const ClearTypeProfile profile = FinishPreviewProfile();
                renderer_.DrawSample(
                    draw->hDC,
                    draw->rcItem,
                    profile,
                    CalibrationStage::PixelStructure,
                    dpi_,
                    dark,
                    false,
                    focused,
                    kSampleText,
                    error);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            const HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, IsDark() ? DarkText() : LightText());
            SetBkColor(dc, IsDark() ? DarkBackground() : LightBackground());
            SetBkMode(dc, OPAQUE);
            return reinterpret_cast<LRESULT>(backgroundBrush_);
        }
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(window_, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, backgroundBrush_);
            return 1;
        }
        case WM_SETTINGCHANGE:
            if (themeMode_ == ThemeMode::System) {
                ApplyTheme();
            }
            return 0;
        case WM_CLOSE:
            CancelAndClose();
            return 0;
        case WM_DESTROY: {
            if (!settingsCommitted_) {
                std::wstring ignoredError;
                settings_.RestorePreview(ignoredError);
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

bool MainWindow::IsBaseDark() const noexcept {
    return IsDarkTheme(themeMode_);
}

bool MainWindow::IsDark() const noexcept {
    return ResolvePreviewDark(themeMode_, WindowsAppsUseLightTheme(), comparePolarity_);
}

}  // namespace ctt::win32
