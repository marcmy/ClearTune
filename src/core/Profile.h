#pragma once

namespace ctt {

struct ClearTypeProfile {
    int pixelStructure{1};
    int gammaLevel{2200};
    int clearTypeLevel{100};
    int textContrastLevel{0};
    int enhancedContrastLevel{50};
    int grayscaleEnhancedContrastLevel{100};

    friend bool operator==(const ClearTypeProfile&, const ClearTypeProfile&) = default;
};

}  // namespace ctt
