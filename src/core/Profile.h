#pragma once

#include <algorithm>

namespace ctt {

struct ClearTypeProfile {
    int pixelStructure{1};
    int gammaLevel{1800};
    int clearTypeLevel{100};
    int textContrastLevel{1};
    int enhancedContrastLevel{50};
    int grayscaleEnhancedContrastLevel{100};

    friend bool operator==(const ClearTypeProfile&, const ClearTypeProfile&) = default;
};

[[nodiscard]] constexpr ClearTypeProfile MakeStockWorkingProfile(
    const ClearTypeProfile& capturedProfile,
    const bool capturedGammaPresent,
    const int monitorGammaLevel) noexcept {
    // The launch snapshot exists for rollback, not as the calibration baseline.
    // Stock cttune starts from its known working defaults, retaining only the
    // monitor's pixel structure and its own gamma when one is already stored.
    ClearTypeProfile workingProfile;
    workingProfile.pixelStructure = capturedProfile.pixelStructure == 2 ? 2 : 1;
    if (capturedGammaPresent && capturedProfile.gammaLevel >= 1000) {
        workingProfile.gammaLevel = std::clamp(capturedProfile.gammaLevel, 1000, 5000);
    } else {
        workingProfile.gammaLevel = monitorGammaLevel >= 1000
            ? std::clamp(monitorGammaLevel, 1000, 5000)
            : 1800;
    }
    return workingProfile;
}

}  // namespace ctt
