#pragma once

#include "core/Candidates.h"
#include "core/Profile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <dwrite_1.h>
#include <wrl/client.h>

#include <string>
#include <string_view>

namespace ctt::win32 {

class SampleRenderer {
public:
    [[nodiscard]] bool Initialize(std::wstring& error);
    void DiscardDeviceResources() noexcept;

    [[nodiscard]] bool DrawSample(
        HDC deviceContext,
        const RECT& bounds,
        const ClearTypeProfile& profile,
        CalibrationStage stage,
        unsigned int dpi,
        bool dark,
        bool selected,
        bool focused,
        std::wstring_view text,
        std::wstring& error);

private:
    Microsoft::WRL::ComPtr<IDWriteFactory1> writeFactory_;
    Microsoft::WRL::ComPtr<IDWriteGdiInterop> gdiInterop_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};

}  // namespace ctt::win32
