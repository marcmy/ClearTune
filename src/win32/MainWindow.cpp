#include "win32/MainWindow.h"

#include "core/Candidates.h"
#include "win32/Preferences.h"
#include "win32/ThemeService.h"
#include "win32/Win32Error.h"

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ctt::win32 {
namespace {
constexpr wchar_t kWindowClass[] = L"ClearTune.MainWindow";
constexpr wchar_t kWindowTitle[] = L"ClearTune";
constexpr int kThemeComboId = 100;
constexpr int kClearTypeCheckId = 101;
constexpr int kTuneAllRadioId = 102;
constexpr int kTuneOneRadioId = 103;
constexpr int kMonitorComboId = 104;
constexpr int kBackButtonId = 200;
constexpr int kNextButtonId = IDOK;
constexpr int kCancelButtonId = IDCANCEL;
constexpr int kSampleBaseId = 1000;
constexpr wchar_t kSampleText[] =
    L"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Mauris ornare odio vel risus. "
    L"Maecenas elit metus, pellentesque quis, pretium.\n\n"
    L"Clear text should look even, calm, and easy to read.";

constexpr COLORREF LightBackground() noexcept { return RGB(249, 249, 249); }
constexpr COLORREF LightText() noexcept { return RGB(24, 24, 24); }
constexpr COLORREF DarkBackground() noexcept { return RGB(32, 32, 32); }
constexpr COLORREF DarkText() noexcept { return RGB(242, 242, 242); }

std::wstring StageName(const CalibrationStage stage) {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return L"pixel structure";
        case CalibrationStage::Gamma:
            return L"gamma";
        case CalibrationStage::ClearTypeLevel:
            return L"ClearType level";
        case CalibrationStage::TextContrast:
            return L"text contrast";
        case CalibrationStage::EnhancedContrast:
        default:
            return L"enhanced contrast";
    }
}

int SamplePageNumber(const WizardPage page) noexcept {
    switch (page) {
        case WizardPage::PixelStructure:
            return 1;
        case WizardPage::Gamma:
            return 2;
        case WizardPage::ClearTypeLevel:
            return 3;
        case WizardPage::TextContrast:
            return 4;
        case WizardPage::EnhancedContrast:
            return 5;
        default:
            return 0;
    }
}

}  // namespace

MainWindow::MainWindow(
    HINSTANCE instance,
    std::vector<MonitorDescriptor> monitors,
    ClearTypeSettingsSession& settings,
    const ThemeMode themeMode)
    : instance_(instance),
      monitors_(std::move(monitors)),
      settings_(settings),
      model_(settings.InitialProfiles()),
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
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

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
    return self != nullptr ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
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
            return 0;
        }
        case WM_COMMAND: {
            const int identifier = LOWORD(wParam);
            const int notification = HIWORD(wParam);
            if (identifier == kThemeComboId && notification == CBN_SELCHANGE) {
                ThemeSelectionChanged();
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
                DestroyWindow(window_);
                return 0;
            }
            if (identifier >= kSampleBaseId && identifier < kSampleBaseId + static_cast<int>(sampleButtons_.size()) && notification == BN_CLICKED) {
                SelectSample(static_cast<std::size_t>(identifier - kSampleBaseId));
                return 0;
            }
            break;
        }
        case WM_DRAWITEM: {
            const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            const UINT sampleBaseId = static_cast<UINT>(kSampleBaseId);
            if (draw->CtlID >= sampleBaseId && draw->CtlID < sampleBaseId + static_cast<UINT>(sampleButtons_.size())) {
                const std::size_t index = static_cast<std::size_t>(draw->CtlID - sampleBaseId);
                if (model_.IsSamplePage() && index < CandidateValues(model_.CurrentStage()).size()) {
                    ClearTypeProfile profile = model_.CurrentProfile();
                    ApplyCandidate(profile, model_.CurrentStage(), index);
                    std::wstring error;
                    const bool selected = index == model_.SelectedCandidateIndex();
                    const bool focused = (draw->itemState & ODS_FOCUS) != 0;
                    if (!renderer_.DrawSample(draw->hDC, draw->rcItem, profile, dpi_, IsDark(), selected, focused, kSampleText, error)) {
                        SetBkColor(draw->hDC, IsDark() ? DarkBackground() : LightBackground());
                        SetTextColor(draw->hDC, IsDark() ? DarkText() : LightText());
                        FillRect(draw->hDC, &draw->rcItem, backgroundBrush_);
                        DrawTextW(draw->hDC, L"Unable to render sample", -1, const_cast<RECT*>(&draw->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                    return TRUE;
                }
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
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

bool MainWindow::CreateControls(std::wstring& error) {
    const DWORD staticStyle = WS_CHILD | WS_VISIBLE | SS_NOPREFIX;
    title_ = CreateWindowExW(0, L"STATIC", L"", staticStyle | SS_LEFT, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    instruction_ = CreateWindowExW(0, L"STATIC", L"", staticStyle | SS_LEFT, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    monitorLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    themeLabel_ = CreateWindowExW(0, L"STATIC", L"Theme:", staticStyle | SS_RIGHT, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    themeCombo_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kThemeComboId)),
        instance_,
        nullptr);
    clearTypeCheck_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Turn on &ClearType",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kClearTypeCheckId)),
        instance_,
        nullptr);
    clearTypeDescription_ = CreateWindowExW(
        0,
        L"STATIC",
        L"ClearType makes the text you see on the screen sharper, clearer and easier to read.",
        WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        window_,
        nullptr,
        instance_,
        nullptr);
    tuneAllRadio_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Tune &all active monitors",
        WS_CHILD | WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTuneAllRadioId)),
        instance_,
        nullptr);
    tuneOneRadio_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Tune &one monitor:",
        WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTuneOneRadioId)),
        instance_,
        nullptr);
    monitorCombo_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMonitorComboId)),
        instance_,
        nullptr);
    backButton_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"< &Back",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBackButtonId)),
        instance_,
        nullptr);
    nextButton_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"&Next >",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNextButtonId)),
        instance_,
        nullptr);
    cancelButton_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelButtonId)),
        instance_,
        nullptr);

    for (std::size_t index = 0; index < sampleButtons_.size(); ++index) {
        sampleButtons_[index] = CreateWindowExW(
            0,
            L"BUTTON",
            L"",
            WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            0,
            0,
            0,
            0,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSampleBaseId + static_cast<int>(index))),
            instance_,
            nullptr);
    }

    if (title_ == nullptr || instruction_ == nullptr || monitorLabel_ == nullptr || themeLabel_ == nullptr ||
        themeCombo_ == nullptr || clearTypeCheck_ == nullptr || clearTypeDescription_ == nullptr ||
        tuneAllRadio_ == nullptr || tuneOneRadio_ == nullptr || monitorCombo_ == nullptr ||
        backButton_ == nullptr || nextButton_ == nullptr || cancelButton_ == nullptr ||
        std::any_of(sampleButtons_.begin(), sampleButtons_.end(), [](HWND handle) { return handle == nullptr; })) {
        error = MakeWindowsError(L"Unable to create wizard controls", GetLastError());
        return false;
    }

    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"System"));
    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
    SendMessageW(themeCombo_, CB_SETCURSEL, static_cast<WPARAM>(ThemeModeToStoredValue(themeMode_)), 0);
    SendMessageW(clearTypeCheck_, BM_SETCHECK, clearTypeEnabled_ ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(tuneAllRadio_, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(tuneOneRadio_, BM_SETCHECK, BST_UNCHECKED, 0);

    for (std::size_t index = 0; index < monitors_.size(); ++index) {
        const std::wstring label = MonitorChoiceLabel(index);
        SendMessageW(monitorCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!monitors_.empty()) {
        SendMessageW(monitorCombo_, CB_SETCURSEL, 0, 0);
    }

    CreateFonts();
    UpdateWelcomeControls();
    return true;
}

void MainWindow::CreateFonts() {
    DestroyFonts();
    const int normalHeight = -MulDiv(9, static_cast<int>(dpi_), 72);
    const int titleHeight = -MulDiv(16, static_cast<int>(dpi_), 72);
    normalFont_ = CreateFontW(
        normalHeight,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    titleFont_ = CreateFontW(
        titleHeight,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    const std::array<HWND, 16> controls{
        instruction_, monitorLabel_, themeLabel_, themeCombo_, clearTypeCheck_, clearTypeDescription_,
        tuneAllRadio_, tuneOneRadio_, monitorCombo_, backButton_, nextButton_, cancelButton_,
        sampleButtons_[0], sampleButtons_[1], sampleButtons_[2], sampleButtons_[3]};
    for (const HWND control : controls) {
        if (control != nullptr) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
        }
    }
    for (std::size_t index = 4; index < sampleButtons_.size(); ++index) {
        SendMessageW(sampleButtons_[index], WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    }
    SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
}

void MainWindow::DestroyFonts() noexcept {
    if (normalFont_ != nullptr) {
        DeleteObject(normalFont_);
        normalFont_ = nullptr;
    }
    if (titleFont_ != nullptr) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
}

void MainWindow::RecreateBackgroundBrush() noexcept {
    if (backgroundBrush_ != nullptr) {
        DeleteObject(backgroundBrush_);
    }
    backgroundBrush_ = CreateSolidBrush(IsDark() ? DarkBackground() : LightBackground());
}

void MainWindow::ApplyTheme() {
    RecreateBackgroundBrush();
    ApplyWindowTheme(window_, themeMode_);
    InvalidateRect(window_, nullptr, TRUE);
    for (const HWND button : sampleButtons_) {
        InvalidateRect(button, nullptr, TRUE);
    }
}

int MainWindow::Scale(const int value) const noexcept {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

void MainWindow::Layout() {
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = Scale(28);
    const int controlHeight = Scale(26);
    const int buttonWidth = Scale(92);
    const int buttonGap = Scale(10);

    MoveWindow(title_, margin, Scale(22), width - margin * 2 - Scale(220), Scale(62), TRUE);
    MoveWindow(themeLabel_, width - margin - Scale(205), Scale(29), Scale(55), controlHeight, TRUE);
    MoveWindow(themeCombo_, width - margin - Scale(145), Scale(25), Scale(145), Scale(160), TRUE);
    MoveWindow(instruction_, margin, Scale(92), width - margin * 2, Scale(58), TRUE);
    MoveWindow(monitorLabel_, margin, Scale(150), width - margin * 2, Scale(42), TRUE);

    MoveWindow(clearTypeCheck_, margin, Scale(186), Scale(250), controlHeight, TRUE);
    MoveWindow(clearTypeDescription_, margin + Scale(24), Scale(218), width - margin * 2 - Scale(24), Scale(44), TRUE);
    MoveWindow(tuneAllRadio_, margin, Scale(282), Scale(300), controlHeight, TRUE);
    MoveWindow(tuneOneRadio_, margin, Scale(318), Scale(300), controlHeight, TRUE);
    MoveWindow(monitorCombo_, margin + Scale(24), Scale(350), std::min(Scale(520), width - margin * 2 - Scale(24)), Scale(200), TRUE);

    const int buttonsY = height - margin - controlHeight;
    MoveWindow(cancelButton_, width - margin - buttonWidth, buttonsY, buttonWidth, controlHeight, TRUE);
    MoveWindow(nextButton_, width - margin - buttonWidth * 2 - buttonGap, buttonsY, buttonWidth, controlHeight, TRUE);
    MoveWindow(backButton_, width - margin - buttonWidth * 3 - buttonGap * 2, buttonsY, buttonWidth, controlHeight, TRUE);

    if (!model_.IsSamplePage()) {
        return;
    }

    const std::size_t count = CandidateValues(model_.CurrentStage()).size();
    const int contentTop = Scale(196);
    const int contentBottom = buttonsY - Scale(24);
    const int contentHeight = std::max(Scale(120), contentBottom - contentTop);
    const int contentWidth = width - margin * 2;
    const int gap = Scale(12);

    if (count == 2) {
        const int cardWidth = (contentWidth - gap) / 2;
        for (std::size_t index = 0; index < count; ++index) {
            MoveWindow(sampleButtons_[index], margin + static_cast<int>(index) * (cardWidth + gap), contentTop, cardWidth, contentHeight, TRUE);
        }
    } else if (count == 3) {
        const int cardWidth = (contentWidth - gap * 2) / 3;
        for (std::size_t index = 0; index < count; ++index) {
            MoveWindow(sampleButtons_[index], margin + static_cast<int>(index) * (cardWidth + gap), contentTop, cardWidth, contentHeight, TRUE);
        }
    } else {
        const int cardWidth = (contentWidth - gap * 2) / 3;
        const int cardHeight = (contentHeight - gap) / 2;
        for (std::size_t index = 0; index < count; ++index) {
            const int column = static_cast<int>(index % 3);
            const int row = static_cast<int>(index / 3);
            MoveWindow(sampleButtons_[index], margin + column * (cardWidth + gap), contentTop + row * (cardHeight + gap), cardWidth, cardHeight, TRUE);
        }
    }
}

void MainWindow::SetControlVisible(HWND control, const bool visible) const noexcept {
    ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

void MainWindow::SetText(HWND control, const std::wstring& text) const noexcept {
    SetWindowTextW(control, text.c_str());
}

void MainWindow::UpdateWelcomeControls() {
    const bool allowMonitorSelection = clearTypeEnabled_ && monitors_.size() > 1;
    const bool tuneOne = SendMessageW(tuneOneRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(tuneAllRadio_, allowMonitorSelection ? TRUE : FALSE);
    EnableWindow(tuneOneRadio_, allowMonitorSelection ? TRUE : FALSE);
    EnableWindow(monitorCombo_, allowMonitorSelection && tuneOne ? TRUE : FALSE);
}

const MonitorDescriptor* MainWindow::CurrentMonitor() const noexcept {
    const std::size_t modelIndex = model_.CurrentMonitorIndex();
    if (modelIndex >= activeMonitorIndices_.size()) {
        return nullptr;
    }
    const std::size_t physicalIndex = activeMonitorIndices_[modelIndex];
    if (physicalIndex >= monitors_.size()) {
        return nullptr;
    }
    return &monitors_[physicalIndex];
}

bool MainWindow::CurrentMonitorNeedsWarning() const noexcept {
    const MonitorDescriptor* monitor = CurrentMonitor();
    return monitor != nullptr &&
        (monitor->portrait || (monitor->nativeResolutionKnown && !monitor->atNativeResolution));
}

std::wstring MainWindow::MonitorChoiceLabel(const std::size_t monitorIndex) const {
    if (monitorIndex >= monitors_.size()) {
        return {};
    }
    const auto& monitor = monitors_[monitorIndex];
    std::wstring label = L"Display ";
    label.append(std::to_wstring(monitorIndex + 1)).append(L": ").append(monitor.friendlyName);
    label.append(L" — ").append(std::to_wstring(monitor.width)).append(L" × ").append(std::to_wstring(monitor.height));
    if (monitor.primary) {
        label.append(L" — primary");
    }
    return label;
}

std::wstring MainWindow::CurrentMonitorDescription() const {
    const MonitorDescriptor* monitor = CurrentMonitor();
    if (monitor == nullptr) {
        return {};
    }
    std::wstring description = L"Display ";
    description.append(std::to_wstring(model_.CurrentMonitorIndex() + 1));
    description.append(L" of ").append(std::to_wstring(model_.MonitorCount())).append(L": ");
    description.append(monitor->friendlyName);
    description.append(L" — ").append(std::to_wstring(monitor->width)).append(L" × ").append(std::to_wstring(monitor->height));
    description.append(monitor->portrait ? L" (portrait)" : L" (landscape)");
    if (monitor->primary) {
        description.append(L" — primary");
    }
    if (monitor->nativeResolutionKnown && !monitor->atNativeResolution) {
        description.append(L" — preferred ").append(std::to_wstring(monitor->nativeWidth));
        description.append(L" × ").append(std::to_wstring(monitor->nativeHeight));
    }
    return description;
}

std::wstring MainWindow::ResolutionWarningText() const {
    const MonitorDescriptor* monitor = CurrentMonitor();
    if (monitor == nullptr) {
        return L"Review this monitor's display setup before continuing.";
    }
    const bool nonNative = monitor->nativeResolutionKnown && !monitor->atNativeResolution;
    if (monitor->portrait && nonNative) {
        return L"This display is rotated and is not using its preferred resolution. ClearType subpixel results can be less consistent in portrait mode, and text usually looks best at the panel's preferred resolution. You can continue without changing either setting.";
    }
    if (monitor->portrait) {
        return L"This display is in portrait orientation. ClearType subpixel rendering is designed around a horizontal RGB or BGR stripe layout, so results can be less consistent after rotation. Continue and choose the samples that look best on this display.";
    }
    return L"This display is not using its preferred resolution. Text usually looks sharpest at the panel's preferred mode. ClearTune will not change the resolution automatically.";
}

std::wstring MainWindow::SampleInstruction() const {
    const int page = SamplePageNumber(model_.CurrentPage());
    std::wstring text = L"Sample ";
    text.append(std::to_wstring(page)).append(L" of 5. This page tunes ");
    text.append(StageName(model_.CurrentStage())).append(L".");
    return text;
}

void MainWindow::MoveToCurrentMonitor() {
    const MonitorDescriptor* monitor = CurrentMonitor();
    if (monitor == nullptr || positionedMonitor_ == model_.CurrentMonitorIndex()) {
        return;
    }

    RECT windowRect{};
    if (GetWindowRect(window_, &windowRect) == FALSE) {
        return;
    }
    MONITORINFO info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    if (GetMonitorInfoW(monitor->handle, &info) == FALSE) {
        return;
    }

    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    const int x = info.rcWork.left + std::max(0, (workWidth - width) / 2);
    const int y = info.rcWork.top + std::max(0, (workHeight - height) / 2);
    SetWindowPos(window_, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
    positionedMonitor_ = model_.CurrentMonitorIndex();
}

void MainWindow::RefreshPage() {
    const WizardPage page = model_.CurrentPage();
    const bool welcomePage = page == WizardPage::Welcome;
    const bool samplePage = model_.IsSamplePage();
    const bool multipleMonitors = monitors_.size() > 1;
    if (page >= WizardPage::Resolution && page <= WizardPage::MonitorComplete) {
        MoveToCurrentMonitor();
    }

    SetControlVisible(clearTypeCheck_, welcomePage);
    SetControlVisible(clearTypeDescription_, welcomePage);
    SetControlVisible(tuneAllRadio_, welcomePage && multipleMonitors);
    SetControlVisible(tuneOneRadio_, welcomePage && multipleMonitors);
    SetControlVisible(monitorCombo_, welcomePage && multipleMonitors);
    SetControlVisible(monitorLabel_, page >= WizardPage::Resolution && page <= WizardPage::MonitorComplete);
    for (std::size_t index = 0; index < sampleButtons_.size(); ++index) {
        const bool show = samplePage && index < CandidateValues(model_.CurrentStage()).size();
        SetControlVisible(sampleButtons_[index], show);
    }

    EnableWindow(backButton_, page != WizardPage::Welcome);
    SetWindowTextW(nextButton_, page == WizardPage::Finish ? L"&Finish" : L"&Next >");

    switch (page) {
        case WizardPage::Welcome:
            SetText(title_, L"Make the text on your screen easier to read");
            SetText(instruction_, multipleMonitors
                ? L"Choose whether to tune every active monitor or just one. No settings are changed until you click Finish."
                : L"No settings are changed until you click Finish.");
            SetText(monitorLabel_, L"");
            UpdateWelcomeControls();
            break;
        case WizardPage::Resolution:
            SetText(title_, L"Check this monitor's display setup");
            SetText(instruction_, ResolutionWarningText());
            SetText(monitorLabel_, CurrentMonitorDescription());
            break;
        case WizardPage::PixelStructure:
        case WizardPage::Gamma:
        case WizardPage::ClearTypeLevel:
        case WizardPage::TextContrast:
        case WizardPage::EnhancedContrast:
            SetText(title_, L"Click the text sample that looks best to you");
            SetText(instruction_, SampleInstruction());
            SetText(monitorLabel_, CurrentMonitorDescription());
            RefreshSampleButtons();
            break;
        case WizardPage::MonitorComplete:
            SetText(title_, L"You have finished tuning this monitor");
            if (model_.CurrentMonitorIndex() + 1 < model_.MonitorCount()) {
                SetText(instruction_, L"Click Next to continue with the next selected monitor.");
            } else {
                SetText(instruction_, L"Click Next to review and apply the selected settings.");
            }
            SetText(monitorLabel_, CurrentMonitorDescription());
            break;
        case WizardPage::Finish:
            if (clearTypeEnabled_) {
                SetText(title_, model_.MonitorCount() == 1
                    ? L"You have finished tuning this monitor"
                    : L"You have finished tuning your monitors");
                SetText(instruction_, L"Click Finish to apply the selected settings. If any write fails, the original ClearType configuration will be restored automatically.");
            } else {
                SetText(title_, L"ClearType will be turned off");
                SetText(instruction_, L"Click Finish to turn off ClearType. Your existing per-monitor calibration values will be preserved.");
            }
            SetText(monitorLabel_, L"");
            break;
    }

    Layout();
    InvalidateRect(window_, nullptr, TRUE);
}

void MainWindow::RefreshSampleButtons() {
    for (const HWND button : sampleButtons_) {
        InvalidateRect(button, nullptr, TRUE);
    }
}

void MainWindow::PrepareSelectedMonitors() {
    activeMonitorIndices_.clear();
    const bool tuneAll = monitors_.size() <= 1 || SendMessageW(tuneAllRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (tuneAll) {
        for (std::size_t index = 0; index < monitors_.size(); ++index) {
            activeMonitorIndices_.push_back(index);
        }
    } else {
        const LRESULT selection = SendMessageW(monitorCombo_, CB_GETCURSEL, 0, 0);
        const std::size_t selectedIndex = selection == CB_ERR ? 0U : static_cast<std::size_t>(selection);
        activeMonitorIndices_.push_back(std::min(selectedIndex, monitors_.size() - 1));
    }

    const auto& initialProfiles = settings_.InitialProfiles();
    std::vector<ClearTypeProfile> profiles;
    profiles.reserve(activeMonitorIndices_.size());
    for (const std::size_t monitorIndex : activeMonitorIndices_) {
        profiles.push_back(monitorIndex < initialProfiles.size() ? initialProfiles[monitorIndex] : ClearTypeProfile{});
    }
    model_ = WizardModel(std::move(profiles));
    positionedMonitor_ = static_cast<std::size_t>(-1);
}

void MainWindow::SkipUnneededResolutionPage(const bool movingForward) {
    if (model_.CurrentPage() != WizardPage::Resolution || CurrentMonitorNeedsWarning()) {
        return;
    }
    if (movingForward) {
        model_.Next();
    } else {
        model_.Back();
    }
}

void MainWindow::NavigateNext() {
    if (model_.CurrentPage() == WizardPage::Welcome) {
        clearTypeEnabled_ = SendMessageW(clearTypeCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!clearTypeEnabled_) {
            model_.FinishNow();
            RefreshPage();
            return;
        }
        PrepareSelectedMonitors();
        model_.Next();
        SkipUnneededResolutionPage(true);
        RefreshPage();
        return;
    }
    if (model_.CurrentPage() == WizardPage::Finish) {
        std::wstring error;
        if (!ApplySettings(error)) {
            MessageBoxW(window_, error.c_str(), L"Unable to apply ClearType settings", MB_OK | MB_ICONERROR);
            return;
        }
        DestroyWindow(window_);
        return;
    }
    model_.Next();
    SkipUnneededResolutionPage(true);
    RefreshPage();
}

void MainWindow::NavigateBack() {
    if (model_.Back()) {
        SkipUnneededResolutionPage(false);
        RefreshPage();
    }
}

void MainWindow::SelectSample(const std::size_t index) {
    model_.SelectCandidate(index);
    RefreshSampleButtons();
}

void MainWindow::ThemeSelectionChanged() {
    const LRESULT selection = SendMessageW(themeCombo_, CB_GETCURSEL, 0, 0);
    themeMode_ = ThemeModeFromStoredValue(selection == CB_ERR ? 0 : static_cast<int>(selection));
    std::wstring error;
    if (!SaveThemeMode(themeMode_, error)) {
        MessageBoxW(window_, error.c_str(), L"Unable to save theme preference", MB_OK | MB_ICONWARNING);
    }
    ApplyTheme();
}

void MainWindow::MonitorSelectionChanged() {
    if (SendMessageW(tuneOneRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        SendMessageW(monitorCombo_, CB_GETCURSEL, 0, 0) == CB_ERR && !monitors_.empty()) {
        SendMessageW(monitorCombo_, CB_SETCURSEL, 0, 0);
    }
    UpdateWelcomeControls();
}

bool MainWindow::ApplySettings(std::wstring& error) {
    const auto connected = EnumerateActiveMonitors();
    std::unordered_set<std::wstring> connectedKeys;
    connectedKeys.reserve(connected.size());
    for (const auto& monitor : connected) {
        connectedKeys.insert(monitor.displayKey);
    }

    std::vector<ApplyTarget> targets;
    bool skippedMonitor = false;
    const auto& profiles = model_.Profiles();
    for (std::size_t modelIndex = 0; modelIndex < activeMonitorIndices_.size() && modelIndex < profiles.size(); ++modelIndex) {
        const std::size_t physicalIndex = activeMonitorIndices_[modelIndex];
        if (physicalIndex >= monitors_.size()) {
            skippedMonitor = true;
            continue;
        }
        const auto& monitor = monitors_[physicalIndex];
        if (!connectedKeys.contains(monitor.displayKey)) {
            skippedMonitor = true;
            continue;
        }
        targets.push_back(ApplyTarget{monitor.displayKey, profiles[modelIndex], monitor.primary});
    }
    if (targets.empty()) {
        error = L"None of the monitors from this tuning session are still connected.";
        return false;
    }
    if (!settings_.Apply(targets, clearTypeEnabled_, error)) {
        return false;
    }
    if (skippedMonitor) {
        MessageBoxW(
            window_,
            L"One or more monitors were disconnected during tuning. Their pending settings were skipped.",
            L"Monitor disconnected",
            MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

bool MainWindow::IsDark() const noexcept {
    return IsDarkTheme(themeMode_);
}

}  // namespace ctt::win32
