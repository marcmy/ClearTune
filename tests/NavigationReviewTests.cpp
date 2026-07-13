#include "core/WizardModel.h"

#include <cstdlib>
#include <iostream>

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

    const ClearTypeProfile baseline{
        .pixelStructure = 1,
        .gammaLevel = 1600,
        .clearTypeLevel = 100,
        .textContrastLevel = 1,
        .enhancedContrastLevel = 50,
        .grayscaleEnhancedContrastLevel = 100,
    };

    WizardModel wizard({baseline}, 1400);
    CHECK(wizard.Next());  // Welcome -> Resolution
    CHECK(wizard.Next());  // Resolution -> Pixel structure
    wizard.SelectCandidate(1);
    CHECK(wizard.Next());  // Pixel structure -> Global contrast
    wizard.SelectCandidate(4);  // Global 1800
    CHECK(wizard.Next());  // Global contrast -> ClearType level
    wizard.SelectCandidate(2);  // ClearType 0
    CHECK(wizard.Next());  // ClearType level -> Contrast combination
    wizard.SelectCandidate(5);  // Enhanced 400 / text contrast 2
    CHECK(wizard.Next());  // Contrast combination -> Grayscale contrast
    wizard.SelectCandidate(3);  // Grayscale 200
    CHECK(wizard.Next());  // Grayscale contrast -> Finish

    CHECK(wizard.CurrentPage() == WizardPage::Finish);
    const ClearTypeProfile expectedReview{
        .pixelStructure = 2,
        .gammaLevel = 1600,
        .clearTypeLevel = 0,
        .textContrastLevel = 2,
        .enhancedContrastLevel = 400,
        .grayscaleEnhancedContrastLevel = 200,
    };
    CHECK(wizard.ReviewProfile() == expectedReview);
    CHECK(wizard.GlobalContrast() == 1800);

    // Back from Finish reverses the page-5 Next: its selected grayscale value
    // disappears, while the earlier committed pages remain in force.
    CHECK(wizard.Back());
    CHECK(wizard.CurrentPage() == WizardPage::GrayscaleEnhancedContrast);
    const ClearTypeProfile expectedPage5Entry{
        .pixelStructure = 2,
        .gammaLevel = 1600,
        .clearTypeLevel = 0,
        .textContrastLevel = 2,
        .enhancedContrastLevel = 400,
        .grayscaleEnhancedContrastLevel = 100,
    };
    CHECK(wizard.CurrentProfile() == expectedPage5Entry);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Navigation review tests passed\n";
    return EXIT_SUCCESS;
}
