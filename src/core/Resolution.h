#pragma once

namespace ctt {

// Windows reports an EDID/DisplayConfig "preferred" mode, but tools such as
// CRU can intentionally make a lower custom timing preferred. Treat the
// current mode as acceptable when it meets or exceeds that preferred pixel
// grid. This preserves the useful warning for genuinely reduced modes while
// avoiding false warnings for native-or-larger custom configurations.
[[nodiscard]] constexpr bool MeetsOrExceedsPreferredResolution(
    const unsigned int currentWidth,
    const unsigned int currentHeight,
    const unsigned int preferredWidth,
    const unsigned int preferredHeight,
    const bool portrait) noexcept {
    if (preferredWidth == 0U || preferredHeight == 0U) {
        return false;
    }

    const unsigned int expectedWidth = portrait ? preferredHeight : preferredWidth;
    const unsigned int expectedHeight = portrait ? preferredWidth : preferredHeight;
    return currentWidth >= expectedWidth && currentHeight >= expectedHeight;
}

}  // namespace ctt
