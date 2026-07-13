#include "win32/MainWindow.h"

#include "win32/Preferences.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace ctt::win32 {

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

    const auto& capturedProfiles = settings_.InitialProfiles();
    std::vector<ClearTypeProfile> profiles;
    profiles.reserve(activeMonitorIndices_.size());
    for (const std::size_t monitorIndex : activeMonitorIndices_) {
        const ClearTypeProfile captured =
            monitorIndex < capturedProfiles.size() ? capturedProfiles[monitorIndex] : ClearTypeProfile{};
        const int monitorGamma =
            monitorIndex < monitors_.size() && monitors_[monitorIndex].gammaKnown
                ? monitors_[monitorIndex].gammaLevel
                : 1800;
        profiles.push_back(MakeStockWorkingProfile(captured, monitorGamma));
    }
    model_ = WizardModel(std::move(profiles), settings_.InitialGlobalContrast());
    positionedMonitor_ = static_cast<std::size_t>(-1);
    comparePolarity_ = false;
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

bool MainWindow::PreviewCurrentProfile(std::wstring& error) {
    if (model_.CurrentPage() == WizardPage::Welcome || activeMonitorIndices_.empty()) {
        return true;
    }
    return settings_.Preview(
        model_.CurrentRenderingProfile(),
        clearTypeEnabled_,
        model_.GlobalContrast(),
        error);
}

void MainWindow::NavigateNext() {
    if (model_.CurrentPage() == WizardPage::Welcome) {
        clearTypeEnabled_ = SendMessageW(clearTypeCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!clearTypeEnabled_) {
            std::wstring error;
            if (!settings_.Preview(model_.CurrentRenderingProfile(), false, model_.GlobalContrast(), error)) {
                MessageBoxW(window_, error.c_str(), L"Unable to preview ClearType settings", MB_OK | MB_ICONERROR);
                return;
            }
            model_.FinishNow();
            RefreshPage();
            return;
        }

        PrepareSelectedMonitors();
        std::wstring error;
        if (!PreviewCurrentProfile(error)) {
            MessageBoxW(window_, error.c_str(), L"Unable to preview ClearType settings", MB_OK | MB_ICONERROR);
            return;
        }
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
        settingsCommitted_ = true;
        DestroyWindow(window_);
        return;
    }

    const std::size_t previousMonitor = model_.CurrentMonitorIndex();
    if (model_.IsSamplePage() || model_.CurrentPage() == WizardPage::Resolution) {
        std::wstring error;
        if (!PreviewCurrentProfile(error)) {
            MessageBoxW(window_, error.c_str(), L"Unable to preview ClearType settings", MB_OK | MB_ICONERROR);
            return;
        }
    }

    model_.Next();
    comparePolarity_ = false;
    SkipUnneededResolutionPage(true);
    if (model_.CurrentPage() != WizardPage::Finish && model_.CurrentMonitorIndex() != previousMonitor) {
        std::wstring error;
        if (!PreviewCurrentProfile(error)) {
            model_.Back();
            MessageBoxW(window_, error.c_str(), L"Unable to preview ClearType settings", MB_OK | MB_ICONERROR);
            return;
        }
    }
    ApplyTheme();
    RefreshPage();
}

void MainWindow::NavigateBack() {
    if (!model_.Back()) {
        return;
    }
    comparePolarity_ = false;
    SkipUnneededResolutionPage(false);

    std::wstring error;
    if (model_.CurrentPage() == WizardPage::Welcome) {
        if (!settings_.RestorePreview(error)) {
            MessageBoxW(window_, error.c_str(), L"Unable to restore ClearType preview", MB_OK | MB_ICONWARNING);
        }
    } else if (!PreviewCurrentProfile(error)) {
        MessageBoxW(window_, error.c_str(), L"Unable to preview ClearType settings", MB_OK | MB_ICONERROR);
    }
    ApplyTheme();
    RefreshPage();
}

void MainWindow::SelectSample(const std::size_t index) {
    model_.SelectCandidate(index);
    RefreshSampleButtons();
}

void MainWindow::ThemeSelectionChanged() {
    const LRESULT selection = SendMessageW(themeCombo_, CB_GETCURSEL, 0, 0);
    themeMode_ = ThemeModeFromStoredValue(selection == CB_ERR ? 0 : static_cast<int>(selection));
    comparePolarity_ = false;
    std::wstring error;
    if (!SaveThemeMode(themeMode_, error)) {
        MessageBoxW(window_, error.c_str(), L"Unable to save theme preference", MB_OK | MB_ICONWARNING);
    }
    ApplyTheme();
    RefreshPage();
}

void MainWindow::ComparePolarity() {
    if (!model_.IsSamplePage()) {
        return;
    }
    comparePolarity_ = !comparePolarity_;
    ApplyTheme();
    RefreshPage();
}

void MainWindow::MonitorSelectionChanged() {
    if (SendMessageW(tuneOneRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        SendMessageW(monitorCombo_, CB_GETCURSEL, 0, 0) == CB_ERR && !monitors_.empty()) {
        SendMessageW(monitorCombo_, CB_SETCURSEL, 0, 0);
    }
    UpdateWelcomeControls();
}

void MainWindow::CancelAndClose() {
    if (closing_) {
        return;
    }
    closing_ = true;
    if (!settingsCommitted_) {
        std::wstring error;
        if (!settings_.RestorePreview(error)) {
            MessageBoxW(window_, error.c_str(), L"Unable to restore ClearType settings", MB_OK | MB_ICONWARNING);
        }
    }
    DestroyWindow(window_);
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
    for (std::size_t modelIndex = 0;
         modelIndex < activeMonitorIndices_.size() && modelIndex < profiles.size();
         ++modelIndex) {
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
        ClearTypeProfile profile = profiles[modelIndex];
        profile.gammaLevel = model_.GlobalContrast();
        targets.push_back(ApplyTarget{monitor.displayKey, profile, monitor.primary});
    }
    if (targets.empty()) {
        error = L"None of the monitors from this tuning session are still connected.";
        return false;
    }
    if (!settings_.Apply(targets, clearTypeEnabled_, model_.GlobalContrast(), error)) {
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

}  // namespace ctt::win32
