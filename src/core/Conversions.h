#pragma once

#include "core/Profile.h"

namespace ctt {

struct RenderingParameters {
    float gamma{2.2F};
    float enhancedContrast{0.5F};
    float clearTypeLevel{1.0F};
    int pixelGeometry{1};
};

[[nodiscard]] RenderingParameters ToRenderingParameters(const ClearTypeProfile& profile) noexcept;
[[nodiscard]] unsigned int ToSpiContrast(const ClearTypeProfile& profile) noexcept;
[[nodiscard]] unsigned int ToSpiOrientation(const ClearTypeProfile& profile) noexcept;

}  // namespace ctt
