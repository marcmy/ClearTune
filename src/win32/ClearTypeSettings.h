#pragma once

#include "core/Profile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace ctt::win32 {

struct OptionalDword {
    bool present{false};
    DWORD value{0};
};

struct PerDisplaySnapshot {
    std::wstring displayKey;
    bool keyExisted{false};
    OptionalDword gammaLevel;
    OptionalDword pixelStructure;
    OptionalDword clearTypeLevel;
    OptionalDword textContrastLevel;
    OptionalDword enhancedContrastLevel;
    OptionalDword grayscaleEnhancedContrastLevel;
};

struct GlobalSmoothingSnapshot {
    BOOL enabled{TRUE};
    UINT type{FE_FONTSMOOTHINGCLEARTYPE};
    UINT contrast{1400};
    UINT orientation{FE_FONTSMOOTHINGORIENTATIONRGB};
};

struct ApplyTarget {
    std::wstring displayKey;
    ClearTypeProfile profile;
    bool primary{false};
};

class ClearTypeSettingsSession {
public:
    [[nodiscard]] bool Capture(std::span<const std::wstring> displayKeys, std::wstring& error);
    [[nodiscard]] bool Preview(
        const ClearTypeProfile& profile,
        bool enableClearType,
        int globalContrast,
        std::wstring& error);
    [[nodiscard]] bool RestorePreview(std::wstring& error);
    [[nodiscard]] bool Apply(
        std::span<const ApplyTarget> targets,
        bool enableClearType,
        int globalContrast,
        std::wstring& error);
    [[nodiscard]] bool Restore(std::wstring& error);

    [[nodiscard]] const std::vector<ClearTypeProfile>& InitialProfiles() const noexcept;
    [[nodiscard]] bool InitialGammaPresent(std::size_t displayIndex) const noexcept;
    [[nodiscard]] bool InitialClearTypeEnabled() const noexcept;
    [[nodiscard]] int InitialGlobalContrast() const noexcept;

private:
    [[nodiscard]] bool CaptureGlobal(std::wstring& error);
    [[nodiscard]] bool ApplyGlobal(
        const ClearTypeProfile& primaryProfile,
        bool enableClearType,
        int globalContrast,
        UINT flags,
        std::wstring& error);
    [[nodiscard]] bool RestoreGlobal(UINT flags, std::wstring& error);
    [[nodiscard]] bool WriteDisplayProfile(const ApplyTarget& target, std::wstring& error);
    [[nodiscard]] bool RestoreDisplay(const PerDisplaySnapshot& snapshot, std::wstring& error);

    GlobalSmoothingSnapshot global_{};
    std::vector<PerDisplaySnapshot> displays_;
    std::vector<ClearTypeProfile> initialProfiles_;
    bool captured_{false};
    bool previewActive_{false};
};

}  // namespace ctt::win32
