#pragma once

#include "core/MonitorLayout.h"
#include "core/Theme.h"
#include "core/WizardModel.h"
#include "win32/ClearTypeSettings.h"
#include "win32/MonitorService.h"
#include "win32/SampleRenderer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ctt::win32 {

class MainWindow {
public:
    MainWindow(
        HINSTANCE instance,
        std::vector<MonitorDescriptor> monitors,
        ClearTypeSettingsSession& settings,
        ThemeMode themeMode);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    [[nodiscard]] bool CreateAndShow(int showCommand, std::wstring& error);
    [[nodiscard]] HWND Handle() const noexcept;

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MonitorMapSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    [[nodiscard]] bool RegisterWindowClass(std::wstring& error);
    [[nodiscard]] bool CreateControls(std::wstring& error);
    void CreateFonts();
    void DestroyFonts() noexcept;
    void RecreateBackgroundBrush() noexcept;
    void ApplyTheme();
    void Layout();
    void RefreshPage();
    void MoveToCurrentMonitor();
    void RefreshSampleButtons();
    void RefreshFinishPreviews();
    void UpdateWelcomeControls();
    void UpdateCompareButton();
    void SetControlVisible(HWND control, bool visible) const noexcept;
    void SetText(HWND control, const std::wstring& text) const noexcept;
    void NavigateNext();
    void NavigateBack();
    void SelectSample(std::size_t index);
    void ThemeSelectionChanged();
    void ComparePolarity();
    void MonitorSelectionChanged();
    void PrepareSelectedMonitors();
    void SkipUnneededResolutionPage(bool movingForward);
    void CancelAndClose();
    void DrawMonitorMap(const DRAWITEMSTRUCT& draw);
    void RebuildMonitorMapLayout();
    void SelectMonitorFromMap(std::size_t index);
    void MoveMonitorMapSelection(int direction);
    void SetTuneOneMonitor(bool enabled);
    [[nodiscard]] std::optional<std::size_t> MonitorMapHitTest(int x, int y) const noexcept;
    [[nodiscard]] bool PreviewCurrentProfile(std::wstring& error);
    [[nodiscard]] bool ApplySettings(std::wstring& error);
    [[nodiscard]] const MonitorDescriptor* CurrentMonitor() const noexcept;
    [[nodiscard]] bool CurrentMonitorNeedsWarning() const noexcept;
    [[nodiscard]] std::wstring CurrentMonitorDescription() const;
    [[nodiscard]] std::wstring ResolutionWarningText() const;
    [[nodiscard]] std::wstring SampleInstruction() const;
    [[nodiscard]] ClearTypeProfile FinishPreviewProfile() const noexcept;
    [[nodiscard]] bool IsBaseDark() const noexcept;
    [[nodiscard]] bool IsDark() const noexcept;
    [[nodiscard]] int Scale(int value) const noexcept;

    HINSTANCE instance_{};
    HWND window_{};
    HWND title_{};
    HWND instruction_{};
    HWND monitorLabel_{};
    HWND themeLabel_{};
    HWND themeCombo_{};
    HWND compareButton_{};
    HWND clearTypeCheck_{};
    HWND clearTypeDescription_{};
    HWND tuneAllRadio_{};
    HWND tuneOneRadio_{};
    HWND monitorMap_{};
    HWND finishLightLabel_{};
    HWND finishDarkLabel_{};
    HWND finishLightPreview_{};
    HWND finishDarkPreview_{};
    HWND backButton_{};
    HWND nextButton_{};
    HWND cancelButton_{};
    std::array<HWND, 6> sampleButtons_{};

    HFONT normalFont_{};
    HFONT titleFont_{};
    HBRUSH backgroundBrush_{};
    UINT dpi_{96};

    std::vector<MonitorDescriptor> monitors_;
    std::vector<std::size_t> activeMonitorIndices_;
    std::vector<MonitorLayoutItem> monitorMapLayout_;
    std::optional<std::size_t> hoveredMonitorIndex_;
    std::size_t selectedMonitorIndex_{0};
    ClearTypeSettingsSession& settings_;
    WizardModel model_;
    ThemeMode themeMode_{ThemeMode::System};
    bool comparePolarity_{false};
    bool clearTypeEnabled_{true};
    bool settingsCommitted_{false};
    bool closing_{false};
    bool monitorMapTracking_{false};
    std::size_t positionedMonitor_{static_cast<std::size_t>(-1)};
    SampleRenderer renderer_;
};

}  // namespace ctt::win32
