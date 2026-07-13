#include "core/Candidates.h"
#include "core/Conversions.h"
#include "core/DisplayKey.h"
#include "core/MonitorLayout.h"
#include "core/Profile.h"
#include "core/Resolution.h"
#include "core/Theme.h"
#include "core/WizardModel.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

int main() {
    using ctt::ThemeMode;
    CHECK(ctt::ThemeModeFromStoredValue(0) == ThemeMode::System);
    CHECK(ctt::ThemeModeFromStoredValue(1) == ThemeMode::Light);
    CHECK(ctt::ThemeModeFromStoredValue(2) == ThemeMode::Dark);
    CHECK(ctt::ThemeModeFromStoredValue(99) == ThemeMode::System);
    CHECK(ctt::ThemeModeToStoredValue(ThemeMode::Dark) == 2);

    CHECK(ctt::ResolveDarkTheme(ThemeMode::System, false));
    CHECK(!ctt::ResolveDarkTheme(ThemeMode::System, true));
    CHECK(!ctt::ResolveDarkTheme(ThemeMode::Light, false));
    CHECK(ctt::ResolveDarkTheme(ThemeMode::Dark, true));
    CHECK(!ctt::ResolvePreviewDark(ThemeMode::Light, true, false));
    CHECK(ctt::ResolvePreviewDark(ThemeMode::Light, true, true));
    CHECK(ctt::ResolvePreviewDark(ThemeMode::Dark, true, false));
    CHECK(!ctt::ResolvePreviewDark(ThemeMode::Dark, true, true));

    CHECK(ctt::NormalizeDisplayKey(L"\\\\.\\DISPLAY7") == L"DISPLAY7");
    CHECK(ctt::NormalizeDisplayKey(L"DISPLAY2") == L"DISPLAY2");
    CHECK(ctt::NormalizeDisplayKey(L"\\\\?\\DISPLAY3") == L"\\\\?\\DISPLAY3");

    CHECK(ctt::MeetsOrExceedsPreferredResolution(1920, 1080, 1440, 1080, false));
    CHECK(!ctt::MeetsOrExceedsPreferredResolution(1280, 720, 1920, 1080, false));
    CHECK(ctt::MeetsOrExceedsPreferredResolution(1080, 1920, 1920, 1080, true));

    const std::array<ctt::MonitorLayoutInput, 2> monitorInputs{{
        {.left = 0, .top = 0, .width = 1920, .height = 1080},
        {.left = 1920, .top = -420, .width = 1080, .height = 1920},
    }};
    const auto monitorLayout = ctt::BuildMonitorLayout(monitorInputs, 520, 280, 16, 24);
    CHECK(monitorLayout.size() == 2);
    CHECK(monitorLayout[0].monitorRect.left < monitorLayout[1].monitorRect.left);
    CHECK(monitorLayout[1].monitorRect.top < monitorLayout[0].monitorRect.top);
    CHECK(monitorLayout[0].monitorRect.Width() > monitorLayout[0].monitorRect.Height());
    CHECK(monitorLayout[1].monitorRect.Height() > monitorLayout[1].monitorRect.Width());
    CHECK(monitorLayout[0].selectionRect.top < monitorLayout[0].monitorRect.top);
    CHECK(monitorLayout[0].selectionRect.left >= 16);
    CHECK(monitorLayout[1].selectionRect.right <= 504);
    CHECK(monitorLayout[0].selectionRect.bottom <= 264);
    CHECK(monitorLayout[1].selectionRect.bottom <= 264);
    const auto firstHit = ctt::HitTestMonitorLayout(
        monitorLayout,
        monitorLayout[0].monitorRect.left + 2,
        monitorLayout[0].monitorRect.top + 2);
    CHECK(firstHit.has_value() && *firstHit == 0);
    const auto labelHit = ctt::HitTestMonitorLayout(
        monitorLayout,
        monitorLayout[1].selectionRect.left + 2,
        monitorLayout[1].selectionRect.top + 2);
    CHECK(labelHit.has_value() && *labelHit == 1);
    CHECK(!ctt::HitTestMonitorLayout(monitorLayout, 0, 0).has_value());
    CHECK(ctt::BuildMonitorLayout({}, 520, 280, 16, 24).empty());

    using ctt::CalibrationStage;
    using ctt::ClearTypeProfile;

    const ClearTypeProfile capturedProfile{
        .pixelStructure = 2,
        .gammaLevel = 1400,
        .clearTypeLevel = 0,
        .textContrastLevel = 6,
        .enhancedContrastLevel = 400,
        .grayscaleEnhancedContrastLevel = 400,
    };
    const ClearTypeProfile stockWorking = ctt::MakeStockWorkingProfile(capturedProfile, 2200);
    CHECK(stockWorking.pixelStructure == 2);
    CHECK(stockWorking.gammaLevel == 2200);
    CHECK(stockWorking.clearTypeLevel == 100);
    CHECK(stockWorking.textContrastLevel == 1);
    CHECK(stockWorking.enhancedContrastLevel == 50);
    CHECK(stockWorking.grayscaleEnhancedContrastLevel == 100);
    CHECK(ctt::MakeStockWorkingProfile(capturedProfile, 0).gammaLevel == 1800);

    CHECK(ctt::CandidateCount(CalibrationStage::PixelStructure) == 2);
    CHECK(ctt::CandidateCount(CalibrationStage::GlobalContrast) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::ClearTypeLevel) == 3);
    CHECK(ctt::CandidateCount(CalibrationStage::ContrastCombination) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::GrayscaleEnhancedContrast) == 6);

    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 0, false) == 0);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 5, false) == 5);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 0, true) == 0);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 2, true) == 2);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 5, true) == 5);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::ClearTypeLevel, 0, true) == 0);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 6, true) == 6);

    CHECK(ctt::BuildGlobalContrastCandidates(1400) ==
        (std::array<int, 6>{1000, 1200, 1400, 1600, 1800, 2000}));
    CHECK(ctt::BuildGlobalContrastCandidates(1500) ==
        (std::array<int, 6>{1100, 1300, 1500, 1700, 1900, 2100}));
    CHECK(ctt::BuildGlobalContrastCandidates(1800) ==
        (std::array<int, 6>{1200, 1400, 1600, 1800, 2000, 2200}));

    ClearTypeProfile profile{};
    const int originalGamma = profile.gammaLevel;
    ctt::ApplyCandidate(profile, CalibrationStage::PixelStructure, 1);
    CHECK(profile.pixelStructure == 2);
    ctt::ApplyCandidate(profile, CalibrationStage::ClearTypeLevel, 2);
    CHECK(profile.clearTypeLevel == 0);
    ctt::ApplyCandidate(profile, CalibrationStage::ContrastCombination, 5);
    CHECK(profile.enhancedContrastLevel == 400);
    CHECK(profile.textContrastLevel == 2);
    ctt::ApplyCandidate(profile, CalibrationStage::GrayscaleEnhancedContrast, 3);
    CHECK(profile.grayscaleEnhancedContrastLevel == 200);
    CHECK(profile.gammaLevel == originalGamma);

    ClearTypeProfile renderingBase = stockWorking;
    const auto combinationRender = ctt::RenderingProfileForCandidate(
        renderingBase, CalibrationStage::ContrastCombination, 4, 1400);
    CHECK(combinationRender.gammaLevel == 2200);
    CHECK(combinationRender.enhancedContrastLevel == 300);
    CHECK(combinationRender.grayscaleEnhancedContrastLevel == 300);
    CHECK(combinationRender.textContrastLevel == 1);
    const auto grayscaleRender = ctt::RenderingProfileForCandidate(
        renderingBase, CalibrationStage::GrayscaleEnhancedContrast, 5, 1600);
    CHECK(grayscaleRender.gammaLevel == 2200);
    CHECK(grayscaleRender.clearTypeLevel == 0);
    CHECK(grayscaleRender.enhancedContrastLevel == 400);
    CHECK(grayscaleRender.grayscaleEnhancedContrastLevel == 400);

    profile.gammaLevel = 2200;
    profile.clearTypeLevel = 50;
    profile.enhancedContrastLevel = 200;
    profile.grayscaleEnhancedContrastLevel = 300;
    const auto rendering = ctt::ToRenderingParameters(profile);
    CHECK(rendering.gamma > 2.19F && rendering.gamma < 2.21F);
    CHECK(rendering.clearTypeLevel > 0.49F && rendering.clearTypeLevel < 0.51F);
    CHECK(rendering.enhancedContrast > 1.99F && rendering.enhancedContrast < 2.01F);
    CHECK(rendering.grayscaleEnhancedContrast > 2.99F && rendering.grayscaleEnhancedContrast < 3.01F);
    CHECK(rendering.pixelGeometry == 2);
    CHECK(ctt::ToSpiOrientation(ClearTypeProfile{.pixelStructure = 1}) == 1u);
    CHECK(ctt::ToSpiOrientation(ClearTypeProfile{.pixelStructure = 2}) == 0u);

    std::vector<ClearTypeProfile> seededProfiles{
        ctt::MakeStockWorkingProfile(capturedProfile, 2200),
        ctt::MakeStockWorkingProfile(ClearTypeProfile{.pixelStructure = 1}, 1800),
    };
    ctt::WizardModel seededWizard(seededProfiles, 1400);
    CHECK(seededWizard.CurrentProfile().gammaLevel == 2200);
    CHECK(seededWizard.CurrentProfile().enhancedContrastLevel == 50);
    CHECK(seededWizard.CurrentRenderingProfile().gammaLevel == 2200);
    CHECK(seededWizard.CandidateRenderingProfile(0).gammaLevel == 2200);
    CHECK(seededWizard.GlobalContrast() == 1400);

    ctt::WizardModel disabledWizard(2, 1400);
    disabledWizard.FinishNow();
    CHECK(disabledWizard.CurrentPage() == ctt::WizardPage::Finish);
    CHECK(disabledWizard.Back());
    CHECK(disabledWizard.CurrentPage() == ctt::WizardPage::Welcome);

    ctt::WizardModel wizard(seededProfiles, 1400);
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Welcome);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::PixelStructure);
    CHECK(wizard.CandidateRenderingProfile(0).gammaLevel == 2200);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::GlobalContrast);
    wizard.SelectCandidate(4);
    CHECK(wizard.GlobalContrast() == 1800);
    CHECK(wizard.Profiles()[0].gammaLevel == 1800);
    CHECK(wizard.Profiles()[1].gammaLevel == 1800);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::ClearTypeLevel);
    CHECK(wizard.CandidateRenderingProfile(0).gammaLevel == 1800);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::ContrastCombination);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::GrayscaleEnhancedContrast);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentMonitorIndex() == 1);
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::PixelStructure);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::ClearTypeLevel);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::PixelStructure);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentMonitorIndex() == 0);
    CHECK(wizard.CurrentPage() == ctt::WizardPage::GrayscaleEnhancedContrast);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
