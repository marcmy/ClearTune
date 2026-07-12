#pragma once

#include "core/Profile.h"

#include <cstddef>

namespace ctt {

enum class CalibrationStage {
    PixelStructure,
    EnhancedContrast,
    ClearTypeLevel,
    ContrastCombination,
    GrayscaleEnhancedContrast,
};

[[nodiscard]] std::size_t CandidateCount(CalibrationStage stage) noexcept;
[[nodiscard]] std::size_t NearestCandidateIndex(CalibrationStage stage, const ClearTypeProfile& profile) noexcept;
void ApplyCandidate(ClearTypeProfile& profile, CalibrationStage stage, std::size_t index) noexcept;
[[nodiscard]] ClearTypeProfile RenderingProfileForCandidate(
    const ClearTypeProfile& profile,
    CalibrationStage stage,
    std::size_t index) noexcept;

}  // namespace ctt
