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
constexpr int kBackButtonId = 200;
constexpr int kNextButtonId = IDOK;
constexpr int kCancelButtonId = IDCANCEL;
constexpr int kSampleBaseId = 1000;
constexpr wchar_t kSampleText[] =
    L"The quick brown fox jumps over the lazy dog. Clear text should look even, calm, and easy to read.";

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
      clearTypeEnabled_(settings.InitialClearTypeEnabled()) {}

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
    RECT desired{0, 0, MulDiv(760, static_cast<int>(initialDpi), 96), MulDiv(600, static_cast<int>(initialDpi), 96)};
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
    const DWORD staticStyle = WS_CHILD | WS_VISIBLE;
    title_ = CreateWindowExW(0, L"STATIC", L"", staticStyle, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    instruction_ = CreateWindowExW(0, L"STATIC", L"", staticStyle | SS_LEFT, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    monitorLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 0, 0, window_, nullptr, instance_, nullptr);
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
        themeCombo_ == nullptr || clearTypeCheck_ == nullptr || backButton_ == nullptr || nextButton_ == nullptr || cancelButton_ == nullptr ||
        std::any_of(sampleButtons_.begin(), sampleButtons_.end(), [](HWND handle) { return handle == nullptr; })) {
        error = MakeWindowsError(L"Unable to create wizard controls", GetLastError());
        return false;
    }

    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"System"));
    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
    SendMessageW(themeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
    SendMessageW(themeCombo_, CB_SETCURSEL, static_cast<WPARAM>(ThemeModeToStoredValue(themeMode_)), 0);
    SendMessageW(clearTypeCheck_, BM_SETCHECK, clearTypeEnabled_ ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateFonts();
    return true;
}

void MainWindow::CreateFonts() {
    DestroyFonts();
    const int normalHeight = -MulDiv(9, static_cast<int>(dpi_), 72);
    const int titleHeight = -MulDiv(18, static_cast<int>(dpi_), 72);
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

    const std::array<HWND, 12> controls{
        instruction_, monitorLabel_, themeLabel_, themeCombo_, clearTypeCheck_, backButton_, nextButton_, cancelButton_,
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

    MoveWindow(title_, margin, Scale(25), width - margin * 2 - Scale(230), Scale(48), TRUE);
    MoveWindow(themeLabel_, width - margin - Scale(205), Scale(31), Scale(55), controlHeight, TRUE);
    MoveWindow(themeCombo_, width - margin - Scale(145), Scale(27), Scale(145), Scale(160), TRUE);
    MoveWindow(instruction_, margin, Scale(86), width - margin * 2, Scale(62), TRUE);
    MoveWindow(monitorLabel_, margin, Scale(148), width - margin * 2, Scale(36), TRUE);
    MoveWindow(clearTypeCheck_, margin, Scale(208), Scale(250), controlHeight, TRUE);

    const int buttonsY = height - margin - controlHeight;
    MoveWindow(cancelButton_, width - margin - buttonWidth, buttonsY, buttonWidth, controlHeight, TRUE);
    MoveWindow(nextButton_, width - margin - buttonWidth * 2 - buttonGap, buttonsY, buttonWidth, controlHeight, TRUE);
    MoveWindow(backButton_, width - margin - buttonWidth * 3 - buttonGap * 2, buttonsY, buttonWidth, controlHeight, TRUE);

    if (!model_.IsSamplePage()) {
        return;
    }

    const std::size_t count = CandidateValues(model_.CurrentStage()).size();
    const int contentTop = Scale(190);
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

std::wstring MainWindow::CurrentMonitorDescription() const {
    if (monitors_.empty()) {
        return {};
    }
    const auto& monitor = monitors_[std::min(model_.CurrentMonitorIndex(), monitors_.size() - 1)];
    std::wstring description = L"Display ";
    description.append(std::to_wstring(model_.CurrentMonitorIndex() + 1));
    description.append(L" of ").append(std::to_wstring(monitors_.size())).append(L": ");
    description.append(monitor.friendlyName);
    description.append(L" — ").append(std::to_wstring(monitor.width)).append(L" × ").append(std::to_wstring(monitor.height));
    description.append(monitor.portrait ? L" (portrait)" : L" (landscape)");
    if (monitor.primary) {
        description.append(L" — primary");
    }
    return description;
}

std::wstring MainWindow::SampleInstruction() const {
    const int page = SamplePageNumber(model_.CurrentPage());
    std::wstring text = L"Click the text sample that looks best to you (";
    text.append(std::to_wstring(page)).append(L" of 5). This page tunes ");
    text.append(StageName(model_.CurrentStage())).append(L".");
    return text;
}

void MainWindow::MoveToCurrentMonitor() {
    if (monitors_.empty() || model_.CurrentMonitorIndex() >= monitors_.size() ||
        positionedMonitor_ == model_.CurrentMonitorIndex()) {
        return;
    }

    RECT windowRect{};
    if (GetWindowRect(window_, &windowRect) == FALSE) {
        return;
    }
    const auto& monitor = monitors_[model_.CurrentMonitorIndex()];
    MONITORINFO info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    if (GetMonitorInfoW(monitor.handle, &info) == FALSE) {
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
    const bool samplePage = model_.IsSamplePage();
    if (page >= WizardPage::Resolution && page <= WizardPage::MonitorComplete) {
        MoveToCurrentMonitor();
    }
    SetControlVisible(clearTypeCheck_, page == WizardPage::Welcome);
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
            SetText(instruction_, L"This wizard helps you choose ClearType settings using samples that match a light or dark app surface. No settings are changed until you click Finish.");
            SetText(monitorLabel_, L"");
            break;
        case WizardPage::MonitorSelection: {
            SetText(title_, L"Tune your active monitors");
            std::wstring text = L"ClearTune detected ";
            text.append(std::to_wstring(monitors_.size())).append(monitors_.size() == 1 ? L" active monitor. " : L" active monitors. ");
            text.append(L"Each display will be tuned separately, beginning with the primary display.");
            SetText(instruction_, text);
            SetText(monitorLabel_, L"");
            break;
        }
        case WizardPage::Resolution:
            SetText(title_, L"Check this monitor's resolution");
            SetText(instruction_, L"Text generally looks best at the panel's native resolution. This version reports the current mode and does not change display resolution automatically.");
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
            if (model_.CurrentMonitorIndex() + 1 < monitors_.size()) {
                SetText(instruction_, L"Click Next to continue with the next active monitor.");
            } else {
                SetText(instruction_, L"Click Next to review and apply the settings for all tuned monitors.");
            }
            SetText(monitorLabel_, CurrentMonitorDescription());
            break;
        case WizardPage::Finish:
            if (clearTypeEnabled_) {
                SetText(title_, L"You have finished tuning your monitors");
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

void MainWindow::NavigateNext() {
    if (model_.CurrentPage() == WizardPage::Welcome) {
        clearTypeEnabled_ = SendMessageW(clearTypeCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!clearTypeEnabled_) {
            model_.FinishNow();
            RefreshPage();
            return;
        }
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
    RefreshPage();
}

void MainWindow::NavigateBack() {
    if (model_.Back()) {
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
    for (std::size_t index = 0; index < monitors_.size() && index < profiles.size(); ++index) {
        const auto& monitor = monitors_[index];
        if (!connectedKeys.contains(monitor.displayKey)) {
            skippedMonitor = true;
            continue;
        }
        targets.push_back(ApplyTarget{monitor.displayKey, profiles[index], monitor.primary});
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
