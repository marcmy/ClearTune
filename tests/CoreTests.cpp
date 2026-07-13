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
#include <vector>

namespace {
int failures = 0;

void Check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

int main() {
    using ctt::CalibrationStage;
    using ctt::ClearTypeProfile;
    using ctt::ThemeMode;
    using ctt::WizardModel;
    using ctt::WizardPage;

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
    const auto firstHit = ctt::HitTestMonitorLayout(
        monitorLayout,
        monitorLayout[0].monitorRect.left + 2,
        monitorLayout[0].monitorRect.top + 2);
    CHECK(firstHit.has_value() && *firstHit == 0);
    CHECK(!ctt::HitTestMonitorLayout(monitorLayout, 0, 0).has_value());
    CHECK(ctt::BuildMonitorLayout({}, 520, 280, 16, 24).empty());

    const ClearTypeProfile capturedProfile{
        .pixelStructure = 2,
        .gammaLevel = 1400,
        .clearTypeLevel = 0,
        .textContrastLevel = 6,
        .enhancedContrastLevel = 400,
        .grayscaleEnhancedContrastLevel = 400,
    };
    const ClearTypeProfile capturedWorking = ctt::MakeStockWorkingProfile(capturedProfile, true, 2200);
    CHECK(capturedWorking.pixelStructure == 2);
    CHECK(capturedWorking.gammaLevel == 1400);
    CHECK(capturedWorking.clearTypeLevel == 100);
    CHECK(capturedWorking.textContrastLevel == 1);
    CHECK(capturedWorking.enhancedContrastLevel == 50);
    CHECK(capturedWorking.grayscaleEnhancedContrastLevel == 100);
    const ClearTypeProfile edidWorking = ctt::MakeStockWorkingProfile(capturedProfile, false, 2200);
    CHECK(edidWorking.gammaLevel == 2200);
    CHECK(edidWorking.clearTypeLevel == 100);
    CHECK(edidWorking.enhancedContrastLevel == 50);
    CHECK(ctt::MakeStockWorkingProfile(capturedProfile, false, 0).gammaLevel == 1800);

    CHECK(ctt::CandidateCount(CalibrationStage::PixelStructure) == 2);
    CHECK(ctt::CandidateCount(CalibrationStage::GlobalContrast) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::ClearTypeLevel) == 3);
    CHECK(ctt::CandidateCount(CalibrationStage::ContrastCombination) == 6);
    CHECK(ctt::CandidateCount(CalibrationStage::GrayscaleEnhancedContrast) == 6);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 2, false) == 2);
    CHECK(ctt::CandidateIndexForPolarity(CalibrationStage::GlobalContrast, 2, true) == 2);
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

    const auto combinationRender = ctt::RenderingProfileForCandidate(
        capturedWorking, CalibrationStage::ContrastCombination, 4, 1800);
    CHECK(combinationRender.gammaLevel == 1400);
    CHECK(combinationRender.enhancedContrastLevel == 300);
    CHECK(combinationRender.grayscaleEnhancedContrastLevel == 300);
    CHECK(combinationRender.textContrastLevel == 1);
    const auto grayscaleRender = ctt::RenderingProfileForCandidate(
        capturedWorking, CalibrationStage::GrayscaleEnhancedContrast, 5, 1800);
    CHECK(grayscaleRender.gammaLevel == 1400);
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

    const ClearTypeProfile secondCaptured{
        .pixelStructure = 1,
        .gammaLevel = 1600,
        .clearTypeLevel = 50,
        .textContrastLevel = 1,
        .enhancedContrastLevel = 100,
        .grayscaleEnhancedContrastLevel = 200,
    };
    std::vector<ClearTypeProfile> seededProfiles{
        ctt::MakeStockWorkingProfile(capturedProfile, true, 2200),
        ctt::MakeStockWorkingProfile(secondCaptured, true, 1800),
    };

    WizardModel disabledWizard(2, 1400);
    disabledWizard.FinishNow();
    CHECK(disabledWizard.CurrentPage() == WizardPage::Finish);
    CHECK(disabledWizard.Back());
    CHECK(disabledWizard.CurrentPage() == WizardPage::Welcome);

    WizardModel wizard(seededProfiles, 1400);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::Resolution);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::PixelStructure);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::GlobalContrast);
    wizard.SelectCandidate(4);
    CHECK(wizard.GlobalContrast() == 1800);
    CHECK(wizard.Profiles()[0].gammaLevel == 1400);
    CHECK(wizard.Profiles()[1].gammaLevel == 1600);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::ClearTypeLevel);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::ContrastCombination);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::GrayscaleEnhancedContrast);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::MonitorReview);
    CHECK(wizard.CurrentMonitorIndex() == 0);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::Resolution);
    CHECK(wizard.CurrentMonitorIndex() == 1);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::PixelStructure);
    CHECK(wizard.Next());
    CHECK(wizard.CurrentPage() == WizardPage::ClearTypeLevel);
    CHECK(wizard.CandidateRenderingProfile(0).gammaLevel == 1600);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::PixelStructure);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::Resolution);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::MonitorReview);
    CHECK(wizard.CurrentMonitorIndex() == 0);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::GrayscaleEnhancedContrast);

    const ClearTypeProfile reversibleBase{
        .pixelStructure = 1,
        .gammaLevel = 1600,
        .clearTypeLevel = 100,
        .textContrastLevel = 1,
        .enhancedContrastLevel = 50,
        .grayscaleEnhancedContrastLevel = 100,
    };
    WizardModel reversibleWizard({reversibleBase}, 1400);
    CHECK(reversibleWizard.Next());
    CHECK(reversibleWizard.Next());
    reversibleWizard.SelectCandidate(1);
    CHECK(reversibleWizard.Next());
    reversibleWizard.SelectCandidate(4);
    CHECK(reversibleWizard.Next());
    reversibleWizard.SelectCandidate(2);
    CHECK(reversibleWizard.Next());
    reversibleWizard.SelectCandidate(5);
    CHECK(reversibleWizard.Next());
    reversibleWizard.SelectCandidate(5);
    CHECK(reversibleWizard.CurrentProfile().grayscaleEnhancedContrastLevel == 400);
    CHECK(reversibleWizard.Back());
    CHECK(reversibleWizard.CurrentPage() == WizardPage::ContrastCombination);
    CHECK(reversibleWizard.CurrentProfile().grayscaleEnhancedContrastLevel == 100);
    CHECK(reversibleWizard.Back());
    CHECK(reversibleWizard.CurrentPage() == WizardPage::ClearTypeLevel);
    CHECK(reversibleWizard.CurrentProfile().enhancedContrastLevel == 50);
    CHECK(reversibleWizard.Back());
    CHECK(reversibleWizard.CurrentPage() == WizardPage::GlobalContrast);
    CHECK(reversibleWizard.GlobalContrast() == 1400);
    CHECK(reversibleWizard.Back());
    CHECK(reversibleWizard.CurrentPage() == WizardPage::PixelStructure);
    CHECK(reversibleWizard.CurrentProfile().pixelStructure == 1);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
