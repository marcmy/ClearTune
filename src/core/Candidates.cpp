#include "core/Candidates.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

namespace ctt {
namespace {
constexpr std::array<int, 2> kPixelStructure{1, 2};
constexpr std::array<int, 3> kClearTypeLevel{100, 50, 0};
constexpr std::array<int, 6> kCombinationEnhancedContrast{0, 50, 100, 200, 300, 400};
constexpr std::array<int, 6> kCombinationTextContrast{0, 0, 1, 1, 1, 2};
constexpr std::array<int, 6> kGrayscaleEnhancedContrast{0, 50, 100, 200, 300, 400};

template <std::size_t Size>
std::size_t NearestIndex(const std::array<int, Size>& values, const int value) noexcept {
    std::size_t bestIndex = 0;
    int bestDistance = std::abs(values.front() - value);
    for (std::size_t index = 1; index < values.size(); ++index) {
        const int distance = std::abs(values[index] - value);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}
}

ContrastCandidates BuildGlobalContrastCandidates(const int currentContrast) noexcept {
    const int clamped = std::clamp(currentContrast, 1000, 2200);
    if (((clamped / 100) & 1) != 0) {
        return {1100, 1300, 1500, 1700, 1900, 2100};
    }
    if (clamped < 1800) {
        return {1000, 1200, 1400, 1600, 1800, 2000};
    }
    return {1200, 1400, 1600, 1800, 2000, 2200};
}

std::size_t CandidateCount(const CalibrationStage stage) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return kPixelStructure.size();
        case CalibrationStage::ClearTypeLevel:
            return kClearTypeLevel.size();
        case CalibrationStage::GlobalContrast:
        case CalibrationStage::ContrastCombination:
        case CalibrationStage::GrayscaleEnhancedContrast:
        default:
            return kCombinationEnhancedContrast.size();
    }
}

std::size_t CandidateIndexForPolarity(
    const CalibrationStage stage,
    const std::size_t visualIndex,
    const bool dark) noexcept {
    const std::size_t count = CandidateCount(stage);
    if (visualIndex >= count) {
        return visualIndex;
    }

    // Gamma changes apparent stroke weight in the opposite visual direction
    // when foreground and background polarity are inverted. Mirror only the
    // global-contrast page so corresponding Light/Dark cards retain the same
    // thin-to-heavy visual order while still selecting the same gamma values.
    if (dark && stage == CalibrationStage::GlobalContrast) {
        return count - 1U - visualIndex;
    }
    return visualIndex;
}

std::size_t NearestCandidateIndex(
    const CalibrationStage stage,
    const ClearTypeProfile& profile) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return NearestIndex(kPixelStructure, profile.pixelStructure);
        case CalibrationStage::ClearTypeLevel:
            return NearestIndex(kClearTypeLevel, profile.clearTypeLevel);
        case CalibrationStage::GrayscaleEnhancedContrast:
            return NearestIndex(kGrayscaleEnhancedContrast, profile.grayscaleEnhancedContrastLevel);
        case CalibrationStage::ContrastCombination: {
            std::size_t bestIndex = 0;
            int bestDistance = std::numeric_limits<int>::max();
            for (std::size_t index = 0; index < kCombinationEnhancedContrast.size(); ++index) {
                const int distance =
                    std::abs(kCombinationEnhancedContrast[index] - profile.enhancedContrastLevel) +
                    std::abs(kCombinationTextContrast[index] - profile.textContrastLevel) * 100;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = index;
                }
            }
            return bestIndex;
        }
        case CalibrationStage::GlobalContrast:
        default:
            return 0;
    }
}

void ApplyCandidate(
    ClearTypeProfile& profile,
    const CalibrationStage stage,
    const std::size_t index) noexcept {
    if (index >= CandidateCount(stage)) {
        return;
    }

    switch (stage) {
        case CalibrationStage::PixelStructure:
            profile.pixelStructure = kPixelStructure[index];
            break;
        case CalibrationStage::ClearTypeLevel:
            profile.clearTypeLevel = kClearTypeLevel[index];
            break;
        case CalibrationStage::ContrastCombination:
            profile.enhancedContrastLevel = kCombinationEnhancedContrast[index];
            profile.textContrastLevel = kCombinationTextContrast[index];
            break;
        case CalibrationStage::GrayscaleEnhancedContrast:
            profile.grayscaleEnhancedContrastLevel = kGrayscaleEnhancedContrast[index];
            break;
        case CalibrationStage::GlobalContrast:
            break;
    }
}

ClearTypeProfile RenderingProfileForCandidate(
    const ClearTypeProfile& profile,
    const CalibrationStage stage,
    const std::size_t index,
    const int globalContrast) noexcept {
    ClearTypeProfile renderingProfile = profile;

    if (stage == CalibrationStage::GlobalContrast) {
        const auto values = BuildGlobalContrastCandidates(globalContrast);
        if (index < values.size()) {
            renderingProfile.gammaLevel = values[index];
        }
        return renderingProfile;
    }

    // Non-global pages retain the monitor working gamma selected before or on
    // stage 2. Replacing it with SPI font-smoothing contrast made every sample
    // inherit an unrelated, much heavier baseline.
    ApplyCandidate(renderingProfile, stage, index);
    switch (stage) {
        case CalibrationStage::ContrastCombination:
            renderingProfile.grayscaleEnhancedContrastLevel = renderingProfile.enhancedContrastLevel;
            break;
        case CalibrationStage::GrayscaleEnhancedContrast:
            renderingProfile.clearTypeLevel = 0;
            renderingProfile.enhancedContrastLevel = renderingProfile.grayscaleEnhancedContrastLevel;
            break;
        case CalibrationStage::PixelStructure:
        case CalibrationStage::GlobalContrast:
        case CalibrationStage::ClearTypeLevel:
            break;
    }
    return renderingProfile;
}

}  // namespace ctt
