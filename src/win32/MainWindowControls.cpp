#include "win32/MainWindow.h"

#include "win32/ThemeService.h"
#include "win32/Win32Error.h"

#include <commctrl.h>

#include <algorithm>
#include <array>

namespace ctt::win32 {
namespace {
constexpr int kThemeComboId = 100;
constexpr int kClearTypeCheckId = 101;
constexpr int kTuneAllRadioId = 102;
constexpr int kTuneOneRadioId = 103;
constexpr int kMonitorComboId = 104;
constexpr int kBackButtonId = 200;
constexpr int kNextButtonId = IDOK;
constexpr int kCancelButtonId = IDCANCEL;
constexpr int kSampleBaseId = 1000;

constexpr COLORREF LightBackground() noexcept { return RGB(249, 249, 249); }
constexpr COLORREF DarkBackground() noexcept { return RGB(32, 32, 32); }
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

    const std::size_t count = CandidateCount(model_.CurrentStage());
    const int contentTop = Scale(196);
    const int contentBottom = buttonsY - Scale(24);
    const int contentHeight = std::max(Scale(120), contentBottom - contentTop);
    const int contentWidth = width - margin * 2;
    const int gap = Scale(12);

    if (count == 2) {
        const int cardWidth = (contentWidth - gap) / 2;
        for (std::size_t index = 0; index < count; ++index) {
            MoveWindow(
                sampleButtons_[index],
                margin + static_cast<int>(index) * (cardWidth + gap),
                contentTop,
                cardWidth,
                contentHeight,
                TRUE);
        }
    } else if (count == 3) {
        const int cardWidth = (contentWidth - gap * 2) / 3;
        for (std::size_t index = 0; index < count; ++index) {
            MoveWindow(
                sampleButtons_[index],
                margin + static_cast<int>(index) * (cardWidth + gap),
                contentTop,
                cardWidth,
                contentHeight,
                TRUE);
        }
    } else {
        const int cardWidth = (contentWidth - gap * 2) / 3;
        const int cardHeight = (contentHeight - gap) / 2;
        for (std::size_t index = 0; index < count; ++index) {
            const int column = static_cast<int>(index % 3);
            const int row = static_cast<int>(index / 3);
            MoveWindow(
                sampleButtons_[index],
                margin + column * (cardWidth + gap),
                contentTop + row * (cardHeight + gap),
                cardWidth,
                cardHeight,
                TRUE);
        }
    }
}

void MainWindow::SetControlVisible(HWND control, const bool visible) const noexcept {
    ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

void MainWindow::SetText(HWND control, const std::wstring& text) const noexcept {
    SetWindowTextW(control, text.c_str());
}

}  // namespace ctt::win32
