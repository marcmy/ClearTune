#include "win32/ClearTypeSettings.h"

namespace ctt::win32 {

bool ClearTypeSettingsSession::InitialGammaPresent(const std::size_t displayIndex) const noexcept {
    return displayIndex < displays_.size() && displays_[displayIndex].gammaLevel.present;
}

}  // namespace ctt::win32
