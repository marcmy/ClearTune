#include "core/Candidates.h"

#include <array>
#include <cstdlib>
#include <limits>

namespace ctt {
namespace {
constexpr std::array<int, 2> kPixelStructure{1, 2};
constexpr std::array<int, 6> kEnhancedContrast{0, 50, 100, 200, 300, 400};
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

std::size_t CandidateCount(const CalibrationStage stage) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return kPixelStructure.size();
        case CalibrationStage::ClearTypeLevel:
            return kClearTypeLevel.size();
        case CalibrationStage::EnhancedContrast:
        case CalibrationStage::ContrastCombination:
        case CalibrationStage::GrayscaleEnhancedContrast:
        default:
            return kEnhancedContrast.size();
    }
}

std::size_t NearestCandidateIndex(
    const CalibrationStage stage,
    const ClearTypeProfile& profile) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return NearestIndex(kPixelStructure, profile.pixelStructure);
        case CalibrationStage::EnhancedContrast:
            return NearestIndex(kEnhancedContrast, profile.enhancedContrastLevel);
        case CalibrationStage::ClearTypeLevel:
            return NearestIndex(kClearTypeLevel, profile.clearTypeLevel);
        case CalibrationStage::GrayscaleEnhancedContrast:
            return NearestIndex(kGrayscaleEnhancedContrast, profile.grayscaleEnhancedContrastLevel);
        case CalibrationStage::ContrastCombination:
        default: {
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
        case CalibrationStage::EnhancedContrast:
            profile.enhancedContrastLevel = kEnhancedContrast[index];
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
    }
}

ClearTypeProfile RenderingProfileForCandidate(
    const ClearTypeProfile& profile,
    const CalibrationStage stage,
    const std::size_t index) noexcept {
    ClearTypeProfile renderingProfile = profile;
    ApplyCandidate(renderingProfile, stage, index);

    // The stock tuner renders its contrast comparison cards with both contrast
    // channels at the candidate value. Only the stage's intended persisted
    // value is committed when the user advances.
    switch (stage) {
        case CalibrationStage::EnhancedContrast:
        case CalibrationStage::ContrastCombination:
            renderingProfile.grayscaleEnhancedContrastLevel = renderingProfile.enhancedContrastLevel;
            break;
        case CalibrationStage::GrayscaleEnhancedContrast:
            renderingProfile.clearTypeLevel = 0;
            renderingProfile.enhancedContrastLevel = renderingProfile.grayscaleEnhancedContrastLevel;
            break;
        case CalibrationStage::PixelStructure:
        case CalibrationStage::ClearTypeLevel:
            break;
    }
    return renderingProfile;
}

}  // namespace ctt
