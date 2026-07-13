#include "win32/MainWindow.h"

#include "core/Candidates.h"

#include <algorithm>

namespace ctt::win32 {

void MainWindow::UpdateWelcomeControls() {
    const bool allowMonitorSelection = clearTypeEnabled_ && monitors_.size() > 1;
    EnableWindow(tuneAllRadio_, allowMonitorSelection ? TRUE : FALSE);
    EnableWindow(tuneOneRadio_, allowMonitorSelection ? TRUE : FALSE);
    EnableWindow(monitorMap_, allowMonitorSelection ? TRUE : FALSE);
    InvalidateRect(monitorMap_, nullptr, FALSE);
}

void MainWindow::UpdateCompareButton() {
    if (compareButton_ == nullptr) {
        return;
    }
    const bool baseDark = IsBaseDark();
    if (comparePolarity_) {
        SetWindowTextW(compareButton_, baseDark ? L"Return to Dark" : L"Return to Light");
    } else {
        SetWindowTextW(compareButton_, baseDark ? L"Compare Light" : L"Compare Dark");
    }
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
    std::wstring text = L"Sample ";
    text.append(std::to_wstring(model_.CurrentSampleNumber()));
    text.append(L" of ").append(std::to_wstring(model_.CurrentSampleCount())).append(L". ");
    if (IsDark()) {
        text.append(L"Dark polarity is more forgiving, so differences may be subtle. Compare Light to choose the clearest sample, then return to Dark to check comfort, glow, and color fringing.");
    } else {
        text.append(L"Light polarity usually makes differences easiest to see. Choose the clearest sample, then Compare Dark to verify that it remains comfortable without glow or color fringing.");
    }
    return text;
}

ClearTypeProfile MainWindow::FinishPreviewProfile() const noexcept {
    // Use the exact per-monitor profile that Finish will commit. The global
    // page-2 contrast is a separate SPI setting and must not replace Avalon
    // GammaLevel in the final DirectWrite review.
    return model_.ReviewProfile();
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
    const bool finishPreview = page == WizardPage::Finish && clearTypeEnabled_;
    const bool multipleMonitors = monitors_.size() > 1;
    if (page >= WizardPage::Resolution && page <= WizardPage::GrayscaleEnhancedContrast) {
        MoveToCurrentMonitor();
    }

    SetControlVisible(compareButton_, samplePage);
    SetControlVisible(clearTypeCheck_, welcomePage);
    SetControlVisible(clearTypeDescription_, welcomePage);
    SetControlVisible(tuneAllRadio_, welcomePage && multipleMonitors);
    SetControlVisible(tuneOneRadio_, welcomePage && multipleMonitors);
    SetControlVisible(monitorMap_, welcomePage && multipleMonitors);
    SetControlVisible(monitorLabel_, page >= WizardPage::Resolution && page <= WizardPage::GrayscaleEnhancedContrast);
    SetControlVisible(finishLightLabel_, finishPreview);
    SetControlVisible(finishDarkLabel_, finishPreview);
    SetControlVisible(finishLightPreview_, finishPreview);
    SetControlVisible(finishDarkPreview_, finishPreview);
    for (std::size_t index = 0; index < sampleButtons_.size(); ++index) {
        const bool show = samplePage && index < CandidateCount(model_.CurrentStage());
        SetControlVisible(sampleButtons_[index], show);
    }

    EnableWindow(backButton_, page != WizardPage::Welcome);
    SetWindowTextW(nextButton_, page == WizardPage::Finish ? L"&Finish" : L"&Next >");
    UpdateCompareButton();

    switch (page) {
        case WizardPage::Welcome:
            SetText(title_, L"Make the text on your screen easier to read");
            SetText(
                instruction_,
                multipleMonitors
                    ? L"Choose all monitors, or click one in the arrangement below. For calibration, Light is usually most revealing and Dark is useful for verification."
                    : L"Recommended: use Light mode to identify the clearest setting, then switch to Dark to verify comfort and check for glow or color fringing.");
            SetText(monitorLabel_, L"");
            UpdateWelcomeControls();
            break;
        case WizardPage::Resolution:
            SetText(title_, L"Check this monitor's display setup");
            SetText(instruction_, ResolutionWarningText());
            SetText(monitorLabel_, CurrentMonitorDescription());
            break;
        case WizardPage::PixelStructure:
        case WizardPage::GlobalContrast:
        case WizardPage::ClearTypeLevel:
        case WizardPage::ContrastCombination:
        case WizardPage::GrayscaleEnhancedContrast:
            SetText(title_, L"Click the text sample that looks best to you");
            SetText(instruction_, SampleInstruction());
            SetText(monitorLabel_, CurrentMonitorDescription());
            RefreshSampleButtons();
            break;
        case WizardPage::Finish:
            if (clearTypeEnabled_) {
                SetText(title_, model_.MonitorCount() == 1
                    ? L"Review and save this monitor's settings"
                    : L"Review and save your monitor settings");
                SetText(instruction_, L"The same selected profile is shown in both polarities. Light is usually more revealing; Dark confirms that the result remains comfortable. Click Finish to save it.");
                RefreshFinishPreviews();
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

void MainWindow::RefreshFinishPreviews() {
    if (finishLightPreview_ != nullptr) {
        InvalidateRect(finishLightPreview_, nullptr, TRUE);
    }
    if (finishDarkPreview_ != nullptr) {
        InvalidateRect(finishDarkPreview_, nullptr, TRUE);
    }
}

}  // namespace ctt::win32
