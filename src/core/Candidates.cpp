#include "core/Candidates.h"

#include <array>
#include <cstdlib>

namespace ctt {
namespace {
constexpr std::array<int, 2> kPixelStructure{1, 2};
constexpr std::array<int, 3> kGamma{1000, 1400, 2200};
constexpr std::array<int, 6> kClearTypeLevel{0, 20, 40, 60, 80, 100};
constexpr std::array<int, 6> kTextContrast{0, 1, 2, 3, 4, 6};
constexpr std::array<int, 6> kEnhancedContrast{0, 50, 100, 200, 350, 500};
}

std::span<const int> CandidateValues(const CalibrationStage stage) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return kPixelStructure;
        case CalibrationStage::Gamma:
            return kGamma;
        case CalibrationStage::ClearTypeLevel:
            return kClearTypeLevel;
        case CalibrationStage::TextContrast:
            return kTextContrast;
        case CalibrationStage::EnhancedContrast:
        default:
            return kEnhancedContrast;
    }
}

std::size_t NearestCandidateIndex(const CalibrationStage stage, const int value) noexcept {
    const auto values = CandidateValues(stage);
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

int ValueForStage(const ClearTypeProfile& profile, const CalibrationStage stage) noexcept {
    switch (stage) {
        case CalibrationStage::PixelStructure:
            return profile.pixelStructure;
        case CalibrationStage::Gamma:
            return profile.gammaLevel;
        case CalibrationStage::ClearTypeLevel:
            return profile.clearTypeLevel;
        case CalibrationStage::TextContrast:
            return profile.textContrastLevel;
        case CalibrationStage::EnhancedContrast:
        default:
            return profile.enhancedContrastLevel;
    }
}

void ApplyCandidate(ClearTypeProfile& profile, const CalibrationStage stage, const std::size_t index) noexcept {
    const auto values = CandidateValues(stage);
    if (index >= values.size()) {
        return;
    }
    const int value = values[index];
    switch (stage) {
        case CalibrationStage::PixelStructure:
            profile.pixelStructure = value;
            break;
        case CalibrationStage::Gamma:
            profile.gammaLevel = value;
            break;
        case CalibrationStage::ClearTypeLevel:
            profile.clearTypeLevel = value;
            break;
        case CalibrationStage::TextContrast:
            profile.textContrastLevel = value;
            break;
        case CalibrationStage::EnhancedContrast:
            profile.enhancedContrastLevel = value;
            break;
    }
}

}  // namespace ctt
