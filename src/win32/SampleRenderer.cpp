#include "win32/SampleRenderer.h"

#include "core/Conversions.h"
#include "win32/Win32Error.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <new>

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

class BitmapTextRenderer final : public IDWriteTextRenderer {
public:
    BitmapTextRenderer(
        IDWriteBitmapRenderTarget* target,
        IDWriteRenderingParams* renderingParameters,
        const COLORREF textColor) noexcept
        : target_(target), renderingParameters_(renderingParameters), textColor_(textColor) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (IsEqualIID(interfaceId, __uuidof(IUnknown)) ||
            IsEqualIID(interfaceId, __uuidof(IDWritePixelSnapping)) ||
            IsEqualIID(interfaceId, __uuidof(IDWriteTextRenderer))) {
            *object = static_cast<IDWriteTextRenderer*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++referenceCount_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = --referenceCount_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
        if (disabled == nullptr) {
            return E_POINTER;
        }
        *disabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
        if (transform == nullptr) {
            return E_POINTER;
        }
        return target_->GetCurrentTransform(transform);
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override {
        if (pixelsPerDip == nullptr) {
            return E_POINTER;
        }
        *pixelsPerDip = target_->GetPixelsPerDip();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(
        void*,
        const FLOAT baselineOriginX,
        const FLOAT baselineOriginY,
        const DWRITE_MEASURING_MODE measuringMode,
        const DWRITE_GLYPH_RUN* glyphRun,
        const DWRITE_GLYPH_RUN_DESCRIPTION*,
        IUnknown*) override {
        return target_->DrawGlyphRun(
            baselineOriginX,
            baselineOriginY,
            measuringMode,
            glyphRun,
            renderingParameters_.Get(),
            textColor_,
            nullptr);
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(
        void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(
        void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(
        void* clientDrawingContext,
        const FLOAT originX,
        const FLOAT originY,
        IDWriteInlineObject* inlineObject,
        const BOOL isSideways,
        const BOOL isRightToLeft,
        IUnknown* drawingEffect) override {
        if (inlineObject == nullptr) {
            return E_INVALIDARG;
        }
        return inlineObject->Draw(
            clientDrawingContext,
            this,
            originX,
            originY,
            isSideways,
            isRightToLeft,
            drawingEffect);
    }

private:
    std::atomic<ULONG> referenceCount_{1};
    Microsoft::WRL::ComPtr<IDWriteBitmapRenderTarget> target_;
    Microsoft::WRL::ComPtr<IDWriteRenderingParams> renderingParameters_;
    COLORREF textColor_{};
};

void DrawCardBorder(
    HDC deviceContext,
    const RECT& bounds,
    const bool selected,
    const bool focused,
    const COLORREF borderColor) noexcept {
    const int thickness = selected ? 3 : 1;
    HPEN borderPen = CreatePen(PS_SOLID, thickness, borderColor);
    if (borderPen != nullptr) {
        const HGDIOBJ previousPen = SelectObject(deviceContext, borderPen);
        const HGDIOBJ previousBrush = SelectObject(deviceContext, GetStockObject(NULL_BRUSH));
        Rectangle(deviceContext, bounds.left, bounds.top, bounds.right, bounds.bottom);
        SelectObject(deviceContext, previousBrush);
        SelectObject(deviceContext, previousPen);
        DeleteObject(borderPen);
    }

    if (focused) {
        RECT focusRect = bounds;
        InflateRect(&focusRect, -5, -5);
        DrawFocusRect(deviceContext, &focusRect);
    }
}

}  // namespace

bool SampleRenderer::Initialize(std::wstring& error) {
    HRESULT result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory1),
        reinterpret_cast<IUnknown**>(writeFactory_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to initialize DirectWrite 1", static_cast<unsigned long>(result));
        return false;
    }

    result = writeFactory_->GetGdiInterop(gdiInterop_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to initialize DirectWrite GDI interop", static_cast<unsigned long>(result));
        return false;
    }

    // Match the effective stock sample size: 11 typographic points converted
    // to DirectWrite device-independent pixels. The previous 11-DIP experiment
    // was visibly far too small at 100% scaling.
    constexpr FLOAT kSampleFontSize = 11.0F * 96.0F / 72.0F;
    result = writeFactory_->CreateTextFormat(
        L"Calibri",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kSampleFontSize,
        L"en-us",
        textFormat_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample text format", static_cast<unsigned long>(result));
        return false;
    }
    textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return true;
}

void SampleRenderer::DiscardDeviceResources() noexcept {
    // Bitmap render targets are deliberately short-lived and recreated for the
    // destination HDC, matching the stock tuner's GDI-compatible path.
}

bool SampleRenderer::DrawSample(
    HDC deviceContext,
    const RECT& bounds,
    const ClearTypeProfile& profile,
    const CalibrationStage stage,
    const unsigned int dpi,
    const bool dark,
    const bool selected,
    const bool focused,
    const std::wstring_view text,
    std::wstring& error) {
    const LONG widthPixels = bounds.right - bounds.left;
    const LONG heightPixels = bounds.bottom - bounds.top;
    if (deviceContext == nullptr || widthPixels <= 0 || heightPixels <= 0) {
        error = L"The sample drawing surface is invalid.";
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteBitmapRenderTarget> baseTarget;
    HRESULT result = gdiInterop_->CreateBitmapRenderTarget(
        deviceContext,
        static_cast<UINT32>(widthPixels),
        static_cast<UINT32>(heightPixels),
        baseTarget.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample bitmap target", static_cast<unsigned long>(result));
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteBitmapRenderTarget1> target;
    result = baseTarget.As(&target);
    if (FAILED(result)) {
        error = MakeWindowsError(L"DirectWrite bitmap target 1 is unavailable", static_cast<unsigned long>(result));
        return false;
    }

    const FLOAT pixelsPerDip = static_cast<FLOAT>(std::max(dpi, 96U)) / 96.0F;
    result = target->SetPixelsPerDip(pixelsPerDip);
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to set the sample DPI", static_cast<unsigned long>(result));
        return false;
    }
    const DWRITE_MATRIX identity{1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F};
    target->SetCurrentTransform(&identity);

    const bool grayscaleStage = stage == CalibrationStage::GrayscaleEnhancedContrast;
    result = target->SetTextAntialiasMode(
        grayscaleStage ? DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE : DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to set the sample antialiasing mode", static_cast<unsigned long>(result));
        return false;
    }

    const auto parameters = ToRenderingParameters(profile);
    Microsoft::WRL::ComPtr<IDWriteRenderingParams1> renderingParameters;
    result = writeFactory_->CreateCustomRenderingParams(
        std::clamp(parameters.gamma, 1.0F, 2.2F),
        std::clamp(parameters.enhancedContrast, 0.0F, 10.0F),
        std::clamp(parameters.grayscaleEnhancedContrast, 0.0F, 10.0F),
        std::clamp(parameters.clearTypeLevel, 0.0F, 1.0F),
        PixelGeometry(parameters.pixelGeometry),
        DWRITE_RENDERING_MODE_DEFAULT,
        renderingParameters.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create sample rendering parameters", static_cast<unsigned long>(result));
        return false;
    }

    const COLORREF background = dark ? RGB(32, 32, 32) : RGB(250, 250, 250);
    const COLORREF foreground = dark ? RGB(242, 242, 242) : RGB(21, 21, 21);
    const COLORREF border = selected
        ? RGB(0, 120, 212)
        : (dark ? RGB(90, 90, 90) : RGB(181, 181, 181));

    HDC memoryDc = target->GetMemoryDC();
    RECT memoryBounds{0, 0, widthPixels, heightPixels};
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    if (backgroundBrush == nullptr) {
        error = MakeWindowsError(L"Unable to create the sample background brush", GetLastError());
        return false;
    }
    FillRect(memoryDc, &memoryBounds, backgroundBrush);
    DeleteObject(backgroundBrush);

    const FLOAT widthDips = static_cast<FLOAT>(widthPixels) / pixelsPerDip;
    const FLOAT heightDips = static_cast<FLOAT>(heightPixels) / pixelsPerDip;
    constexpr FLOAT kHorizontalPadding = 8.0F;
    constexpr FLOAT kVerticalPadding = 8.0F;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    result = writeFactory_->CreateTextLayout(
        text.data(),
        static_cast<UINT32>(text.size()),
        textFormat_.Get(),
        std::max(1.0F, widthDips - kHorizontalPadding * 2.0F),
        std::max(1.0F, heightDips - kVerticalPadding * 2.0F),
        layout.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to create the sample text layout", static_cast<unsigned long>(result));
        return false;
    }

    auto* textRenderer = new (std::nothrow) BitmapTextRenderer(
        target.Get(), renderingParameters.Get(), foreground);
    if (textRenderer == nullptr) {
        error = L"Unable to allocate the sample text renderer.";
        return false;
    }
    result = layout->Draw(nullptr, textRenderer, kHorizontalPadding, kVerticalPadding);
    textRenderer->Release();
    if (FAILED(result)) {
        error = MakeWindowsError(L"Unable to draw the sample text", static_cast<unsigned long>(result));
        return false;
    }

    if (BitBlt(
            deviceContext,
            bounds.left,
            bounds.top,
            widthPixels,
            heightPixels,
            memoryDc,
            0,
            0,
            SRCCOPY) == FALSE) {
        error = MakeWindowsError(L"Unable to copy the rendered sample", GetLastError());
        return false;
    }

    DrawCardBorder(deviceContext, bounds, selected, focused, border);
    return true;
}

}  // namespace ctt::win32
