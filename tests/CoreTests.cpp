#include "core/Candidates.h"
#include "core/Conversions.h"
#include "core/DisplayKey.h"
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

    using ctt::CalibrationStage;
    using ctt::ClearTypeProfile;
    CHECK(ctt::CandidateValues(CalibrationStage::PixelStructure).size() == 2);
    CHECK(ctt::CandidateValues(CalibrationStage::Gamma).size() == 3);
    CHECK(ctt::CandidateValues(CalibrationStage::ClearTypeLevel).size() == 6);
    CHECK(ctt::CandidateValues(CalibrationStage::TextContrast).size() == 6);
    CHECK(ctt::CandidateValues(CalibrationStage::EnhancedContrast).size() == 6);
    CHECK(ctt::CandidateValues(CalibrationStage::Gamma).front() == 1000);
    CHECK(ctt::CandidateValues(CalibrationStage::EnhancedContrast).back() == 500);
    CHECK(ctt::NearestCandidateIndex(CalibrationStage::Gamma, 2190) == 2);

    ClearTypeProfile profile{};
    ctt::ApplyCandidate(profile, CalibrationStage::PixelStructure, 1);
    CHECK(profile.pixelStructure == 2);
    ctt::ApplyCandidate(profile, CalibrationStage::TextContrast, 5);
    CHECK(profile.textContrastLevel == 6);

    profile.gammaLevel = 2200;
    profile.clearTypeLevel = 80;
    profile.textContrastLevel = 4;
    profile.enhancedContrastLevel = 150;
    const auto rendering = ctt::ToRenderingParameters(profile);
    CHECK(rendering.gamma > 2.19F && rendering.gamma < 2.21F);
    CHECK(rendering.clearTypeLevel > 0.79F && rendering.clearTypeLevel < 0.81F);
    CHECK(rendering.enhancedContrast > 4.49F && rendering.enhancedContrast < 4.51F);
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
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Welcome);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == ctt::WizardPage::Resolution);
    wizard.Next();
    CHECK(wizard.CurrentPage() == ctt::WizardPage::PixelStructure);
    wizard.SelectCandidate(1);
    CHECK(wizard.CurrentProfile().pixelStructure == 2);
    while (!wizard.IsMonitorCompletePage()) {
        CHECK(wizard.Next());
    }
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
