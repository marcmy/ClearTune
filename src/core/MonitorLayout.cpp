#include "core/MonitorLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ctt {

std::vector<MonitorLayoutItem> BuildMonitorLayout(
    const std::span<const MonitorLayoutInput> monitors,
    const int canvasWidth,
    const int canvasHeight,
    const int padding,
    const int labelHeight) noexcept {
    std::vector<MonitorLayoutItem> result;
    if (monitors.empty() || canvasWidth <= 0 || canvasHeight <= 0) {
        return result;
    }

    int minimumLeft = std::numeric_limits<int>::max();
    int minimumTop = std::numeric_limits<int>::max();
    int maximumRight = std::numeric_limits<int>::min();
    int maximumBottom = std::numeric_limits<int>::min();
    for (const auto& monitor : monitors) {
        if (monitor.width <= 0 || monitor.height <= 0) {
            continue;
        }
        minimumLeft = std::min(minimumLeft, monitor.left);
        minimumTop = std::min(minimumTop, monitor.top);
        maximumRight = std::max(maximumRight, monitor.left + monitor.width);
        maximumBottom = std::max(maximumBottom, monitor.top + monitor.height);
    }
    if (minimumLeft == std::numeric_limits<int>::max()) {
        return result;
    }

    const int safePadding = std::max(0, padding);
    const int safeLabelHeight = std::max(0, labelHeight);
    const int desktopWidth = std::max(1, maximumRight - minimumLeft);
    const int desktopHeight = std::max(1, maximumBottom - minimumTop);
    const int availableWidth = std::max(1, canvasWidth - safePadding * 2);
    const int availableBodyHeight = std::max(1, canvasHeight - safePadding * 2 - safeLabelHeight);
    const double scale = std::min(
        static_cast<double>(availableWidth) / static_cast<double>(desktopWidth),
        static_cast<double>(availableBodyHeight) / static_cast<double>(desktopHeight));

    const int groupWidth = static_cast<int>(std::lround(static_cast<double>(desktopWidth) * scale));
    const int groupBodyHeight = static_cast<int>(std::lround(static_cast<double>(desktopHeight) * scale));
    const int originX = std::max(safePadding, (canvasWidth - groupWidth) / 2);
    const int originY = std::max(
        safePadding + safeLabelHeight,
        (canvasHeight - (groupBodyHeight + safeLabelHeight)) / 2 + safeLabelHeight);

    result.reserve(monitors.size());
    for (const auto& monitor : monitors) {
        if (monitor.width <= 0 || monitor.height <= 0) {
            result.push_back({});
            continue;
        }

        const int left = originX + static_cast<int>(std::lround(
            static_cast<double>(monitor.left - minimumLeft) * scale));
        const int top = originY + static_cast<int>(std::lround(
            static_cast<double>(monitor.top - minimumTop) * scale));
        const int right = left + std::max(1, static_cast<int>(std::lround(
            static_cast<double>(monitor.width) * scale)));
        const int bottom = top + std::max(1, static_cast<int>(std::lround(
            static_cast<double>(monitor.height) * scale)));

        const LayoutRect monitorRect{
            .left = left,
            .top = top,
            .right = std::min(canvasWidth - safePadding, right),
            .bottom = std::min(canvasHeight - safePadding, bottom),
        };
        const LayoutRect selectionRect{
            .left = monitorRect.left,
            .top = std::max(safePadding, monitorRect.top - safeLabelHeight),
            .right = monitorRect.right,
            .bottom = monitorRect.bottom,
        };
        result.push_back({monitorRect, selectionRect});
    }
    return result;
}

std::optional<std::size_t> HitTestMonitorLayout(
    const std::span<const MonitorLayoutItem> items,
    const int x,
    const int y) noexcept {
    for (std::size_t offset = 0; offset < items.size(); ++offset) {
        const std::size_t index = items.size() - 1U - offset;
        if (items[index].selectionRect.Contains(x, y)) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace ctt
