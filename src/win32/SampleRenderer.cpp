#include "win32/SampleRenderer.h"

#include "core/Conversions.h"
#include "win32/Win32Error.h"

#include <algorithm>
#include <dxgiformat.h>

namespace ctt::win32 {
namespace {

DWRITE_PIXEL_GEOMETRY PixelGeometry(const int value) noexcept {
    switch (value) {
        case 2:
            return DWRITE_PIXEL_GEOMETRY_BGR;
        case 1:
            return DWRITE_PIXEL_GEOMETRY_RGB;
        default:
            return DWRITE_PIXEL_GEOMETRY_FLAT;
    }
}

}  // namespace

bool SampleRenderer::Initialize(std::wstring& error) {
    HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to initialize Direct2D", static_cast<unsigned long>(result));
        return false;
    }

    result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(writeFactory_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to initialize DirectWrite", static_cast<unsigned long>(result));
        return false;
    }

    result = writeFactory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0F,
        L"en-us",
        textFormat_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample text format", static_cast<unsigned long>(result));
        return false;
    }
    textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return true;
}

bool SampleRenderer::EnsureDeviceResources(std::wstring& error) {
    if (renderTarget_ != nullptr) {
        return true;
    }

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        0.0F,
        0.0F,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
        D2D1_FEATURE_LEVEL_DEFAULT);

    HRESULT result = d2dFactory_->CreateDCRenderTarget(&properties, renderTarget_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample render target", static_cast<unsigned long>(result));
        return false;
    }

    result = renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), textBrush_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample text brush", static_cast<unsigned long>(result));
        DiscardDeviceResources();
        return false;
    }
    result = renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0x0078D4), borderBrush_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample border brush", static_cast<unsigned long>(result));
        DiscardDeviceResources();
        return false;
    }
    return true;
}

void SampleRenderer::DiscardDeviceResources() noexcept {
    borderBrush_.Reset();
    textBrush_.Reset();
    renderTarget_.Reset();
}

bool SampleRenderer::DrawSample(
    HDC deviceContext,
    const RECT& bounds,
    const ClearTypeProfile& profile,
    const unsigned int dpi,
    const bool dark,
    const bool selected,
    const bool focused,
    const std::wstring_view text,
    std::wstring& error) {
    if (deviceContext == nullptr || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        error = L"The sample drawing surface is invalid.";
        return false;
    }
    if (!EnsureDeviceResources(error)) {
        return false;
    }

    const HRESULT bindResult = renderTarget_->BindDC(deviceContext, &bounds);
    if (FAILED(bindResult)) {
        error = MakeWindowsError(L"Unable to bind the sample drawing surface", static_cast<unsigned long>(bindResult));
        return false;
    }

    const auto parameters = ToRenderingParameters(profile);
    Microsoft::WRL::ComPtr<IDWriteRenderingParams> renderingParams;
    const HRESULT parameterResult = writeFactory_->CreateCustomRenderingParams(
        std::clamp(parameters.gamma, 1.0F, 2.2F),
        std::clamp(parameters.enhancedContrast, 0.0F, 10.0F),
        std::clamp(parameters.clearTypeLevel, 0.0F, 1.0F),
        PixelGeometry(parameters.pixelGeometry),
        DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
        renderingParams.ReleaseAndGetAddressOf());
    if (FAILED(parameterResult)) {
        error = MakeWindowsError(L"Unable to create sample rendering parameters", static_cast<unsigned long>(parameterResult));
        return false;
    }

    const float effectiveDpi = static_cast<float>(std::max(dpi, 96U));
    renderTarget_->SetDpi(effectiveDpi, effectiveDpi);
    const float pixelToDip = 96.0F / effectiveDpi;
    const float width = static_cast<float>(bounds.right - bounds.left) * pixelToDip;
    const float height = static_cast<float>(bounds.bottom - bounds.top) * pixelToDip;
    const D2D1_COLOR_F background = dark ? D2D1::ColorF(0x202020) : D2D1::ColorF(0xFAFAFA);
    const D2D1_COLOR_F foreground = dark ? D2D1::ColorF(0xF2F2F2) : D2D1::ColorF(0x151515);
    const D2D1_COLOR_F border = selected ? D2D1::ColorF(0x0078D4) : (dark ? D2D1::ColorF(0x5A5A5A) : D2D1::ColorF(0xB5B5B5));

    textBrush_->SetColor(foreground);
    borderBrush_->SetColor(border);
    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    renderTarget_->SetTextRenderingParams(renderingParams.Get());
    renderTarget_->BeginDraw();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(background);

    const D2D1_RECT_F textBounds = D2D1::RectF(14.0F, 10.0F, std::max(15.0F, width - 14.0F), std::max(11.0F, height - 10.0F));
    renderTarget_->DrawTextW(
        text.data(),
        static_cast<UINT32>(text.size()),
        textFormat_.Get(),
        textBounds,
        textBrush_.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);

    const float borderInset = selected ? 1.5F : 0.5F;
    renderTarget_->DrawRectangle(
        D2D1::RectF(borderInset, borderInset, width - borderInset, height - borderInset),
        borderBrush_.Get(),
        selected ? 3.0F : 1.0F);
    if (focused) {
        Microsoft::WRL::ComPtr<ID2D1StrokeStyle> dashed;
        const D2D1_STROKE_STYLE_PROPERTIES strokeProperties = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_FLAT,
            D2D1_CAP_STYLE_FLAT,
            D2D1_CAP_STYLE_FLAT,
            D2D1_LINE_JOIN_MITER,
            10.0F,
            D2D1_DASH_STYLE_DASH,
            0.0F);
        if (SUCCEEDED(d2dFactory_->CreateStrokeStyle(&strokeProperties, nullptr, 0, dashed.ReleaseAndGetAddressOf()))) {
            renderTarget_->DrawRectangle(D2D1::RectF(5.0F, 5.0F, width - 5.0F, height - 5.0F), borderBrush_.Get(), 1.0F, dashed.Get());
        }
    }

    const HRESULT drawResult = renderTarget_->EndDraw();
    if (drawResult == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        error = L"The sample rendering device was reset.";
        return false;
    }
    if (FAILED(drawResult)) {
        error = MakeWindowsError(L"Unable to draw the sample", static_cast<unsigned long>(drawResult));
        return false;
    }
    return true;
}

}  // namespace ctt::win32
