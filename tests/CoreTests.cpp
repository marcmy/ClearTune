#include "core/Candidates.h"
#include "core/Conversions.h"
#include "core/DisplayKey.h"
#include "core/Resolution.h"
#include "core/Theme.h"
#include "core/WizardModel.h"

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

    CHECK(ctt::NormalizeDisplayKey(L"\\\\.\\DISPLAY7") == L"DISPLAY7");
    CHECK(ctt::NormalizeDisplayKey(L"DISPLAY2") == L"DISPLAY2");
    CHECK(ctt::NormalizeDisplayKey(L"\\\\?\\DISPLAY3") == L"\\\\?\\DISPLAY3");

    CHECK(ctt::MeetsOrExceedsPreferredResolution(1920, 1080, 1440, 1080, false));
    CHECK(!ctt::MeetsOrExceedsPreferredResolution(1280, 720, 1920, 1080, false));
    CHECK(ctt::MeetsOrExceedsPreferredResolution(1080, 1920, 1920, 1080, true));

    using ctt::CalibrationStage;
    using ctt::ClearTypeProfile;
    CHECK(ctt::CandidateCount(CalibrationStage::PixelStructure) == 2);
    CHECK(ctt::CandidateCount(CalibrationStage::EnhancedContrast) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::ClearTypeLevel) == 3);
    CHECK(ctt::CandidateCount(CalibrationStage::ContrastCombination) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::GrayscaleEnhancedContrast) == 6);

    ClearTypeProfile profile{};
    const int originalGamma = profile.gammaLevel;
    ctt::ApplyCandidate(profile, CalibrationStage::PixelStructure, 1);
    CHECK(profile.pixelStructure == 2);
    ctt::ApplyCandidate(profile, CalibrationStage::EnhancedContrast, 5);
    CHECK(profile.enhancedContrastLevel == 400);
    ctt::ApplyCandidate(profile, CalibrationStage::ClearTypeLevel, 2);
    CHECK(profile.clearTypeLevel == 0);
    ctt::ApplyCandidate(profile, CalibrationStage::ContrastCombination, 5);
    CHECK(profile.enhancedContrastLevel == 400);
    CHECK(profile.textContrastLevel == 2);
    ctt::ApplyCandidate(profile, CalibrationStage::GrayscaleEnhancedContrast, 3);
    CHECK(profile.grayscaleEnhancedContrastLevel == 200);
    CHECK(profile.gammaLevel == originalGamma);

    ClearTypeProfile renderingBase{};
    const auto combinationRender = ctt::RenderingProfileForCandidate(
        renderingBase, CalibrationStage::ContrastCombination, 4);
    CHECK(combinationRender.enhancedContrastLevel == 300);
    CHECK(combinationRender.grayscaleEnhancedContrastLevel == 300);
    CHECK(combinationRender.textContrastLevel == 1);
    const auto grayscaleRender = ctt::RenderingProfileForCandidate(
        renderingBase, CalibrationStage::GrayscaleEnhancedContrast, 5);
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
    CHECK(ctt::ToSpiContrast(ClearTypeProfile{.textContrastLevel = 6}) == 2200u);
    CHECK(ctt::ToSpiOrientation(ClearTypeProfile{.pixelStructure = 1}) == 1u);
    CHECK(ctt::ToSpiOrientation(ClearTypeProfile{.pixelStructure = 2}) == 0u);

    std::vector<ClearTypeProfile> seededProfiles(2);
    seededProfiles[0].gammaLevel = 1800;
    seededProfiles[1].gammaLevel = 2200;
    ctt::WizardModel seededWizard(seededProfiles);
    CHECK(seededWizard.CurrentProfile().gammaLevel == 1800);

    ctt::WizardModel disabledWizard(2);
    disabledWizard.FinishNow();
    CHECK(disabledWizard.CurrentPage() == ctt::WizardPage::Finish);
    CHECK(disabledWizard.Back());
    CHECK(disabledWizard.CurrentPage() == ctt::WizardPage::Welcome);

    ctt::WizardModel wizard(2);
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Welcome);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::PixelStructure);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::EnhancedContrast);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::ClearTypeLevel);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::ContrastCombination);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::GrayscaleEnhancedContrast);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::MonitorComplete);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentMonitorIndex() == 1);
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    while (!wizard.IsMonitorCompletePage()) {
        CHECK(wizard.Next());
    }
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Finish);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
