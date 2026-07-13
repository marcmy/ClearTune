#include "core/MonitorIdentity.h"
#include "core/MonitorLayout.h"
#include "core/WizardModel.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
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
    using ctt::ClearTypeProfile;
    using ctt::WizardModel;
    using ctt::WizardPage;

    CHECK(ctt::ChooseMonitorFriendlyName(
        L"Generic PnP Monitor", L"XZ270 X", L"DISPLAY1") == L"XZ270 X");
    CHECK(ctt::ChooseMonitorFriendlyName(
        L"BenQ GW2765", L"", L"DISPLAY2") == L"BenQ GW2765");
    CHECK(ctt::ChooseMonitorFriendlyName(L"", L"", L"DISPLAY3") == L"DISPLAY3");

    const std::array<ctt::MonitorLayoutInput, 2> adjacentMonitors{{
        {.left = 0, .top = 0, .width = 1920, .height = 1080},
        {.left = 1920, .top = -420, .width = 1080, .height = 1920},
    }};
    const auto spacedLayout = ctt::BuildMonitorLayout(
        adjacentMonitors, 520, 280, 20, 24, 12);
    CHECK(spacedLayout.size() == 2);
    CHECK(spacedLayout[1].monitorRect.left - spacedLayout[0].monitorRect.right >= 10);
    CHECK(spacedLayout[0].selectionRect.left >= 20);
    CHECK(spacedLayout[1].selectionRect.right <= 500);

    const ClearTypeProfile first{
        .pixelStructure = 1,
        .gammaLevel = 1600,
        .clearTypeLevel = 100,
        .textContrastLevel = 1,
        .enhancedContrastLevel = 50,
        .grayscaleEnhancedContrastLevel = 100,
    };
    const ClearTypeProfile second{
        .pixelStructure = 2,
        .gammaLevel = 1800,
        .clearTypeLevel = 100,
        .textContrastLevel = 1,
        .enhancedContrastLevel = 50,
        .grayscaleEnhancedContrastLevel = 100,
    };

    WizardModel wizard({first, second}, 1400);
    CHECK(wizard.Next());  // Welcome -> Resolution
    CHECK(wizard.Next());  // Resolution -> Pixel structure
    CHECK(wizard.Next());  // Pixel structure -> Global contrast
    CHECK(wizard.Next());  // Global contrast -> ClearType level
    CHECK(wizard.Next());  // ClearType level -> Contrast combination
    CHECK(wizard.Next());  // Contrast combination -> Grayscale contrast
    wizard.SelectCandidate(3);
    const ClearTypeProfile firstReview = wizard.CurrentProfile();
    CHECK(wizard.Next());  // Grayscale contrast -> first monitor review
    CHECK(wizard.CurrentPage() == WizardPage::MonitorReview);
    CHECK(wizard.CurrentMonitorIndex() == 0);
    CHECK(wizard.ReviewProfile() == firstReview);

    CHECK(wizard.Next());  // Review -> second monitor resolution
    CHECK(wizard.CurrentPage() == WizardPage::Resolution);
    CHECK(wizard.CurrentMonitorIndex() == 1);
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::MonitorReview);
    CHECK(wizard.CurrentMonitorIndex() == 0);
    CHECK(wizard.ReviewProfile() == firstReview);
    CHECK(wizard.Next());

    CHECK(wizard.Next());  // Resolution -> Pixel structure
    CHECK(wizard.Next());  // Pixel structure -> ClearType level
    CHECK(wizard.Next());  // ClearType level -> Contrast combination
    CHECK(wizard.Next());  // Contrast combination -> Grayscale contrast
    wizard.SelectCandidate(4);
    const ClearTypeProfile finalReview = wizard.CurrentProfile();
    CHECK(wizard.Next());  // Last monitor -> final review/save screen
    CHECK(wizard.CurrentPage() == WizardPage::Finish);
    CHECK(wizard.CurrentMonitorIndex() == 1);
    CHECK(wizard.ReviewProfile() == finalReview);

    WizardModel singleMonitor({first}, 1400);
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.Next());
    CHECK(singleMonitor.CurrentPage() == WizardPage::Finish);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Final polish tests passed\n";
    return EXIT_SUCCESS;
}
