#pragma once

#include "core/Profile.h"

#include <array>
#include <cstddef>

namespace ctt {

enum class CalibrationStage {
    PixelStructure,
    GlobalContrast,
    ClearTypeLevel,
    ContrastCombination,
    GrayscaleEnhancedContrast,
};

using ContrastCandidates = std::array<int, 6>;

[[nodiscard]] ContrastCandidates BuildGlobalContrastCandidates(int currentContrast) noexcept;
[[nodiscard]] std::size_t CandidateCount(CalibrationStage stage) noexcept;
[[nodiscard]] std::size_t CandidateIndexForPolarity(
    CalibrationStage stage,
    std::size_t visualIndex,
    bool dark) noexcept;
[[nodiscard]] std::size_t NearestCandidateIndex(
    CalibrationStage stage,
    const ClearTypeProfile& profile) noexcept;
void ApplyCandidate(ClearTypeProfile& profile, CalibrationStage stage, std::size_t index) noexcept;
[[nodiscard]] ClearTypeProfile RenderingProfileForCandidate(
    const ClearTypeProfile& profile,
    CalibrationStage stage,
    std::size_t index,
    int globalContrast) noexcept;

}  // namespace ctt
