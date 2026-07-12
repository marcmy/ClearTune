#pragma once

#include "core/Profile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
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
        unsigned int dpi,
        bool dark,
        bool selected,
        bool focused,
        std::wstring_view text,
        std::wstring& error);

private:
    [[nodiscard]] bool EnsureDeviceResources(std::wstring& error);

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush_;
};

}  // namespace ctt::win32
