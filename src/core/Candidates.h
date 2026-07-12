#pragma once

#include "core/Profile.h"

#include <cstddef>
#include <span>

namespace ctt {

enum class CalibrationStage {
    PixelStructure,
    Gamma,
    ClearTypeLevel,
    TextContrast,
    EnhancedContrast,
};

[[nodiscard]] std::span<const int> CandidateValues(CalibrationStage stage) noexcept;
[[nodiscard]] std::size_t NearestCandidateIndex(CalibrationStage stage, int value) noexcept;
void ApplyCandidate(ClearTypeProfile& profile, CalibrationStage stage, std::size_t index) noexcept;
[[nodiscard]] int ValueForStage(const ClearTypeProfile& profile, CalibrationStage stage) noexcept;

}  // namespace ctt
