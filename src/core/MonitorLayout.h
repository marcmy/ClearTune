#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ctt {

struct LayoutRect {
    int left{};
    int top{};
    int right{};
    int bottom{};

    [[nodiscard]] constexpr int Width() const noexcept { return right - left; }
    [[nodiscard]] constexpr int Height() const noexcept { return bottom - top; }
    [[nodiscard]] constexpr bool Contains(const int x, const int y) const noexcept {
        return x >= left && x < right && y >= top && y < bottom;
    }
};

struct MonitorLayoutInput {
    int left{};
    int top{};
    int width{};
    int height{};
};

struct MonitorLayoutItem {
    LayoutRect monitorRect{};
    LayoutRect selectionRect{};
};

[[nodiscard]] std::vector<MonitorLayoutItem> BuildMonitorLayout(
    std::span<const MonitorLayoutInput> monitors,
    int canvasWidth,
    int canvasHeight,
    int padding,
    int labelHeight) noexcept;

[[nodiscard]] std::optional<std::size_t> HitTestMonitorLayout(
    std::span<const MonitorLayoutItem> items,
    int x,
    int y) noexcept;

}  // namespace ctt
