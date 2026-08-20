/**
 * @file Geometry.hpp
 * @brief   2D 几何基础类型 — Size2T、RectT、Segment2T、Circle2T 及标量别名
 * @details 提供轴对齐矩形、线段、圆等几何原语及对应的 i/f/d 标量别名
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include "Point.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>


namespace SZX::COMMON
{

// Widen integer types to 64-bit for area/length-squared calculations to
// avoid overflow; floating-point types pass through unchanged.
template <PointScalar T>
using widen_t = std::conditional_t<std::integral<T>, std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>, T>;

namespace detail
{
template <PointScalar T>
[[nodiscard]] constexpr widen_t<T> SaturatingNonNegativeProduct(T lhs, T rhs) noexcept
{
    if constexpr (std::integral<T>)
    {
        using W                   = widen_t<T>;
        const long double product = static_cast<long double>(lhs) * static_cast<long double>(rhs);
        const auto maxValue       = static_cast<long double>(std::numeric_limits<W>::max());
        if (product >= maxValue)
            return std::numeric_limits<W>::max();
        return static_cast<W>(product);
    }
    else
    {
        return static_cast<widen_t<T>>(lhs * rhs);
    }
}

template <PointScalar T>
[[nodiscard]] constexpr widen_t<T> SaturatingSquaredDistance2D(T x0, T y0, T x1, T y1) noexcept
{
    const long double dx      = static_cast<long double>(x1) - static_cast<long double>(x0);
    const long double dy      = static_cast<long double>(y1) - static_cast<long double>(y0);
    const long double squared = (dx * dx) + (dy * dy);
    if constexpr (std::integral<T>)
    {
        using W             = widen_t<T>;
        const auto maxValue = static_cast<long double>(std::numeric_limits<W>::max());
        if (squared >= maxValue)
            return std::numeric_limits<W>::max();
        return static_cast<W>(squared);
    }
    else
    {
        return static_cast<widen_t<T>>(squared);
    }
}
} // namespace detail

// ------------------------------------------------------------------------
// Size2T - 2D size (width x height).
// ------------------------------------------------------------------------
template <PointScalar T>
struct Size2T
{
    T width{};
    T height{};

    constexpr Size2T() noexcept = default;

    constexpr Size2T(T widthValue, T heightValue) noexcept :
        width(widthValue),
        height(heightValue)
    {
    }

    bool operator==(const Size2T &other) const noexcept
    {
        return width == other.width && height == other.height;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return width <= T{0} || height <= T{0};
    }

    [[nodiscard]] widen_t<T> Area() const noexcept
    {
        if (IsEmpty())
            return widen_t<T>{0};
        return detail::SaturatingNonNegativeProduct(width, height);
    }
};

// ------------------------------------------------------------------------
// RectT - axis-aligned 2D rectangle (homogeneous scalar).
// Contains() uses half-open semantics: a point on the right or bottom
// edge is NOT inside the rect.
// Coordinates are stored as x1, y1, x2, y2 where x2/y2 are the exclusive
// right/bottom bounds.
// Note: named RectT (not Rect2T) because rectangles are inherently 2D.
// ------------------------------------------------------------------------
template <PointScalar T>
struct RectT
{
    T x1{};
    T y1{};
    T x2{};
    T y2{};

    constexpr RectT() noexcept = default;

    constexpr RectT(T left, T top, T right, T bottom) noexcept :
        x1(left),
        y1(top),
        x2(right),
        y2(bottom)
    {
    }

    constexpr RectT(const Point2T<T> &topLeft, const Size2T<T> &size) noexcept :
        x1(topLeft.x),
        y1(topLeft.y),
        x2(topLeft.x + size.width),
        y2(topLeft.y + size.height)
    {
    }

    constexpr RectT(const Point2T<T> &topLeft, const Point2T<T> &bottomRight) noexcept :
        x1(topLeft.x),
        y1(topLeft.y),
        x2(bottomRight.x),
        y2(bottomRight.y)
    {
    }

    [[nodiscard]] static constexpr RectT FromPoints(const Point2T<T> &a, const Point2T<T> &b) noexcept
    {
        return RectT{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
    }

    bool operator==(const RectT &other) const noexcept
    {
        return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
    }

    [[nodiscard]] T Left() const noexcept
    {
        return x1;
    }
    [[nodiscard]] T Top() const noexcept
    {
        return y1;
    }
    [[nodiscard]] T Right() const noexcept
    {
        return x2;
    }
    [[nodiscard]] T Bottom() const noexcept
    {
        return y2;
    }
    [[nodiscard]] T Width() const noexcept
    {
        return x2 - x1;
    }
    [[nodiscard]] T Height() const noexcept
    {
        return y2 - y1;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return x2 <= x1 || y2 <= y1;
    }

    [[nodiscard]] widen_t<T> Area() const noexcept
    {
        if (IsEmpty())
            return widen_t<T>{0};
        return detail::SaturatingNonNegativeProduct(Width(), Height());
    }

    // Half-open containment: pt.x in [x1, x2), pt.y in [y1, y2).
    [[nodiscard]] bool Contains(const Point2T<T> &pt) const noexcept
    {
        return pt.x >= x1 && pt.x < x2 && pt.y >= y1 && pt.y < y2;
    }

    // Half-open full containment of another rect.
    [[nodiscard]] bool Contains(const RectT &other) const noexcept
    {
        return other.x1 >= x1 && other.y1 >= y1 && other.x2 <= x2 && other.y2 <= y2;
    }

    // Half-open AABB overlap; touching edges do NOT overlap.
    [[nodiscard]] bool Intersects(const RectT &other) const noexcept
    {
        return x1 < other.x2 && other.x1 < x2 && y1 < other.y2 && other.y1 < y2;
    }

    // Returns the intersection rect, or an empty rect at the origin when
    // there is no overlap. Callers should test the result with IsEmpty().
    [[nodiscard]] RectT Intersection(const RectT &other) const noexcept
    {
        const T ix1 = std::max(x1, other.x1);
        const T iy1 = std::max(y1, other.y1);
        const T ix2 = std::min(x2, other.x2);
        const T iy2 = std::min(y2, other.y2);
        if (ix2 <= ix1 || iy2 <= iy1)
        {
            return RectT{T{0}, T{0}, T{0}, T{0}};
        }
        return RectT{ix1, iy1, ix2, iy2};
    }

    // Bounding box of both rects. An empty rect is absorbed (returns the
    // other); if both are empty, returns a default RectT.
    [[nodiscard]] RectT Union(const RectT &other) const noexcept
    {
        if (IsEmpty())
            return other;
        if (other.IsEmpty())
            return *this;
        return RectT{std::min(x1, other.x1), std::min(y1, other.y1), std::max(x2, other.x2), std::max(y2, other.y2)};
    }

    // Expand (or shrink with negative values) the rect by dx on each side
    // horizontally and dy on each side vertically.
    [[nodiscard]] RectT Inflate(T dx, T dy) const noexcept
    {
        return RectT{x1 - dx, y1 - dy, x2 + dx, y2 + dy};
    }

    // Translated copy.
    [[nodiscard]] RectT Offset(T dx, T dy) const noexcept
    {
        return RectT{x1 + dx, y1 + dy, x2 + dx, y2 + dy};
    }
};

// ------------------------------------------------------------------------
// Segment2T - 2D directed line segment between two points.
// ------------------------------------------------------------------------
template <PointScalar T>
struct Segment2T
{
    Point2T<T> start{};
    Point2T<T> end{};

    bool operator==(const Segment2T &other) const noexcept
    {
        return start == other.start && end == other.end;
    }

    [[nodiscard]] double Length() const noexcept
    {
        return distance(start, end);
    }

    [[nodiscard]] widen_t<T> LengthSquared() const noexcept
    {
        return detail::SaturatingSquaredDistance2D(start.x, start.y, end.x, end.y);
    }
};

// ------------------------------------------------------------------------
// Circle2T - 2D circle defined by a center point and a radius.
// Contains() is a closed-disk test (the boundary is included).
// ------------------------------------------------------------------------
template <PointScalar T>
struct Circle2T
{
    Point2T<T> center{};
    T radius{};

    bool operator==(const Circle2T &other) const noexcept
    {
        return center == other.center && radius == other.radius;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return radius <= T{0};
    }

    // Closed-disk containment: distance(center, pt) <= radius.
    // Uses squared distance to avoid sqrt and stay exact for integer T.
    [[nodiscard]] bool Contains(const Point2T<T> &pt) const noexcept
    {
        const long double dx = static_cast<long double>(pt.x) - static_cast<long double>(center.x);
        const long double dy = static_cast<long double>(pt.y) - static_cast<long double>(center.y);
        const long double r  = static_cast<long double>(radius);
        return (dx * dx) + (dy * dy) <= (r * r);
    }

    [[nodiscard]] double Area() const noexcept
    {
        if (IsEmpty())
            return 0.0;
        const auto r = static_cast<double>(radius);
        return std::numbers::pi_v<double> * (r * r);
    }
};

// ------------------------------------------------------------------------
// Scalar aliases.
// ------------------------------------------------------------------------
using Size2i = Size2T<int>;
using Size2f = Size2T<float>;
using Size2d = Size2T<double>;

using Rect2i = RectT<int>;
using Rect2f = RectT<float>;
using Rect2d = RectT<double>;

using Segment2i = Segment2T<int>;
using Segment2f = Segment2T<float>;
using Segment2d = Segment2T<double>;

using Circle2i = Circle2T<int>;
using Circle2f = Circle2T<float>;
using Circle2d = Circle2T<double>;

} // namespace SZX::COMMON
