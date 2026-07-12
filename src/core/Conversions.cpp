#include "core/Conversions.h"

#include <algorithm>

namespace ctt {

RenderingParameters ToRenderingParameters(const ClearTypeProfile& profile) noexcept {
    RenderingParameters result;
    result.gamma = static_cast<float>(std::clamp(profile.gammaLevel, 1000, 2200)) / 1000.0F;
    result.enhancedContrast = static_cast<float>(std::clamp(profile.enhancedContrastLevel, 0, 1000)) / 100.0F;
    result.clearTypeLevel = static_cast<float>(std::clamp(profile.clearTypeLevel, 0, 100)) / 100.0F;
    result.pixelGeometry = profile.pixelStructure == 2 ? 2 : (profile.pixelStructure == 1 ? 1 : 0);
    return result;
}

unsigned int ToSpiContrast(const ClearTypeProfile& profile) noexcept {
    return static_cast<unsigned int>(1000 + std::clamp(profile.textContrastLevel, 0, 6) * 200);
}

unsigned int ToSpiOrientation(const ClearTypeProfile& profile) noexcept {
    return profile.pixelStructure == 2 ? 0U : 1U;
}

}  // namespace ctt
