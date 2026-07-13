#include "win32/MainWindow.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <string>

namespace ctt::win32 {
namespace {

constexpr UINT_PTR kMonitorMapSubclassId = 1;

void DrawOutline(HDC dc, const RECT& rect, const int thickness, const COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ previousPen = SelectObject(dc, pen);
    const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

RECT ToRect(const LayoutRect& rect) noexcept {
    return RECT{rect.left, rect.top, rect.right, rect.bottom};
}

}  // namespace

LRESULT CALLBACK MainWindow::MonitorMapSubclassProcedure(
    HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam,
    const UINT_PTR subclassId,
    const DWORD_PTR referenceData) {
    auto* self = reinterpret_cast<MainWindow*>(referenceData);
    if (self == nullptr) {
        return DefSubclassProc(window, message, wParam, lParam);
    }

    switch (message) {
        case WM_MOUSEMOVE: {
            if (!self->monitorMapTracking_) {
                TRACKMOUSEEVENT tracking{};
                tracking.cbSize = static_cast<DWORD>(sizeof(tracking));
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = window;
                TrackMouseEvent(&tracking);
                self->monitorMapTracking_ = true;
            }
            self->RebuildMonitorMapLayout();
            const auto hovered = self->MonitorMapHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (hovered != self->hoveredMonitorIndex_) {
                self->hoveredMonitorIndex_ = hovered;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            self->monitorMapTracking_ = false;
            self->hoveredMonitorIndex_.reset();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            self->RebuildMonitorMapLayout();
            const auto hit = self->MonitorMapHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (hit) {
                self->SelectMonitorFromMap(*hit);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_LEFT || wParam == VK_UP) {
                self->MoveMonitorMapSelection(-1);
                return 0;
            }
            if (wParam == VK_RIGHT || wParam == VK_DOWN) {
                self->MoveMonitorMapSelection(1);
                return 0;
            }
            if (wParam == VK_SPACE || wParam == VK_RETURN) {
                self->SetTuneOneMonitor(true);
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, MonitorMapSubclassProcedure, subclassId);
            break;
        default:
            break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

void MainWindow::RebuildMonitorMapLayout() {
    if (monitorMap_ == nullptr) {
        monitorMapLayout_.clear();
        return;
    }
    RECT client{};
    GetClientRect(monitorMap_, &client);
    std::vector<MonitorLayoutInput> inputs;
    inputs.reserve(monitors_.size());
    for (const auto& monitor : monitors_) {
        inputs.push_back(MonitorLayoutInput{
            .left = monitor.bounds.left,
            .top = monitor.bounds.top,
            .width = static_cast<int>(monitor.width),
            .height = static_cast<int>(monitor.height),
        });
    }
    monitorMapLayout_ = BuildMonitorLayout(
        inputs,
        client.right - client.left,
        client.bottom - client.top,
        Scale(12),
        Scale(24));
}

std::optional<std::size_t> MainWindow::MonitorMapHitTest(const int x, const int y) const noexcept {
    return HitTestMonitorLayout(monitorMapLayout_, x, y);
}

void MainWindow::SetTuneOneMonitor(const bool enabled) {
    SendMessageW(tuneAllRadio_, BM_SETCHECK, enabled ? BST_UNCHECKED : BST_CHECKED, 0);
    SendMessageW(tuneOneRadio_, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    UpdateWelcomeControls();
}

void MainWindow::SelectMonitorFromMap(const std::size_t index) {
    if (index >= monitors_.size()) {
        return;
    }
    selectedMonitorIndex_ = index;
    SetTuneOneMonitor(true);
    InvalidateRect(monitorMap_, nullptr, FALSE);
}

void MainWindow::MoveMonitorMapSelection(const int direction) {
    if (monitors_.empty()) {
        return;
    }
    const std::size_t count = monitors_.size();
    if (direction < 0) {
        selectedMonitorIndex_ = selectedMonitorIndex_ == 0 ? count - 1U : selectedMonitorIndex_ - 1U;
    } else {
        selectedMonitorIndex_ = (selectedMonitorIndex_ + 1U) % count;
    }
    SetTuneOneMonitor(true);
    InvalidateRect(monitorMap_, nullptr, FALSE);
}

void MainWindow::DrawMonitorMap(const DRAWITEMSTRUCT& draw) {
    RebuildMonitorMapLayout();

    const bool dark = IsDark();
    const bool enabled = IsWindowEnabled(monitorMap_) != FALSE;
    const COLORREF background = dark ? RGB(32, 32, 32) : RGB(249, 249, 249);
    const COLORREF text = enabled
        ? (dark ? RGB(242, 242, 242) : RGB(24, 24, 24))
        : (dark ? RGB(125, 125, 125) : RGB(145, 145, 145));
    const COLORREF frame = dark ? RGB(105, 105, 105) : RGB(145, 145, 145);
    const COLORREF screen = enabled
        ? (dark ? RGB(52, 71, 108) : RGB(91, 118, 177))
        : (dark ? RGB(52, 52, 52) : RGB(205, 205, 205));
    const COLORREF accent = RGB(0, 120, 212);

    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(draw.hDC, &draw.rcItem, backgroundBrush);
    DeleteObject(backgroundBrush);

    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, text);
    const HGDIOBJ previousFont = SelectObject(draw.hDC, normalFont_);

    const bool tuneOne = SendMessageW(tuneOneRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    for (std::size_t index = 0; index < monitorMapLayout_.size() && index < monitors_.size(); ++index) {
        const auto& item = monitorMapLayout_[index];
        RECT labelRect = ToRect(item.selectionRect);
        labelRect.bottom = item.monitorRect.top - Scale(2);
        std::wstring label = monitors_[index].friendlyName.empty()
            ? L"Display " + std::to_wstring(index + 1U)
            : monitors_[index].friendlyName;
        DrawTextW(
            draw.hDC,
            label.c_str(),
            -1,
            &labelRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        RECT monitorRect = ToRect(item.monitorRect);
        HBRUSH frameBrush = CreateSolidBrush(frame);
        FillRect(draw.hDC, &monitorRect, frameBrush);
        DeleteObject(frameBrush);

        RECT screenRect = monitorRect;
        InflateRect(&screenRect, -Scale(4), -Scale(4));
        if (screenRect.right > screenRect.left && screenRect.bottom > screenRect.top) {
            HBRUSH screenBrush = CreateSolidBrush(screen);
            FillRect(draw.hDC, &screenRect, screenBrush);
            DeleteObject(screenBrush);
        }

        RECT outlineRect = ToRect(item.selectionRect);
        InflateRect(&outlineRect, Scale(2), Scale(2));
        const bool selected = tuneOne && index == selectedMonitorIndex_;
        const bool hovered = hoveredMonitorIndex_ && *hoveredMonitorIndex_ == index;
        if (selected) {
            DrawOutline(draw.hDC, outlineRect, Scale(2), accent);
        } else if (hovered) {
            DrawOutline(draw.hDC, outlineRect, 1, accent);
        }
    }

    SelectObject(draw.hDC, previousFont);
    if ((draw.itemState & ODS_FOCUS) != 0) {
        RECT focus = draw.rcItem;
        InflateRect(&focus, -Scale(2), -Scale(2));
        DrawFocusRect(draw.hDC, &focus);
    }
}

}  // namespace ctt::win32
