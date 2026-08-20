/**
 * @file Point.hpp
 * @brief   2D / 3D 点类型模板 — Point2T / Point3T 及标量别名
 * @details 提供点类型定义及 distance、vector_between 等几何辅助函数
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>


namespace SZX::COMMON
{

template <typename T>
concept PointScalar = ((std::integral<T> || std::floating_point<T>) && !std::same_as<std::remove_cv_t<T>, bool>);

namespace detail
{
template <PointScalar T>
[[nodiscard]] constexpr T SaturatingSubtract(T lhs, T rhs) noexcept
{
    if constexpr (std::floating_point<T>)
    {
        return lhs - rhs;
    }
    else
    {
        const long double diff     = static_cast<long double>(lhs) - static_cast<long double>(rhs);
        const long double minValue = static_cast<long double>(std::numeric_limits<T>::min());
        const long double maxValue = static_cast<long double>(std::numeric_limits<T>::max());
        if (diff <= minValue)
            return std::numeric_limits<T>::min();
        if (diff >= maxValue)
            return std::numeric_limits<T>::max();
        return static_cast<T>(diff);
    }
}
} // namespace detail

template <PointScalar T>
struct Point2T
{
    T x{};
    T y{};

    constexpr Point2T() noexcept = default;

    constexpr Point2T(T xValue, T yValue) noexcept :
        x(xValue),
        y(yValue)
    {
    }

    bool operator==(const Point2T &other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

template <PointScalar T>
struct Point3T
{
    T x{};
    T y{};
    T z{};

    constexpr Point3T() noexcept = default;

    constexpr Point3T(T xValue, T yValue, T zValue) noexcept :
        x(xValue),
        y(yValue),
        z(zValue)
    {
    }

    bool operator==(const Point3T &other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

template <PointScalar T>
double distance(const Point2T<T> &p) noexcept
{
    return std::hypot(static_cast<double>(p.x), static_cast<double>(p.y));
}

template <PointScalar T>
Point2T<T> vector_between(const Point2T<T> &from, const Point2T<T> &to) noexcept
{
    // Result type intentionally matches the input scalar. Unsigned negative
    // deltas saturate to 0 instead of wrapping.
    return Point2T<T>{detail::SaturatingSubtract(to.x, from.x), detail::SaturatingSubtract(to.y, from.y)};
}

template <PointScalar T>
double distance(const Point2T<T> &from, const Point2T<T> &to) noexcept
{
    const auto dx = static_cast<double>(to.x) - static_cast<double>(from.x);
    const auto dy = static_cast<double>(to.y) - static_cast<double>(from.y);
    return std::hypot(dx, dy);
}

template <PointScalar T>
double distance(const Point3T<T> &p) noexcept
{
    return std::hypot(static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z));
}

template <PointScalar T>
Point3T<T> vector_between(const Point3T<T> &from, const Point3T<T> &to) noexcept
{
    // Result type intentionally matches the input scalar. Unsigned negative
    // deltas saturate to 0 instead of wrapping.
    return Point3T<T>{detail::SaturatingSubtract(to.x, from.x), detail::SaturatingSubtract(to.y, from.y),
                      detail::SaturatingSubtract(to.z, from.z)};
}

template <PointScalar T>
double distance(const Point3T<T> &from, const Point3T<T> &to) noexcept
{
    const auto dx = static_cast<double>(to.x) - static_cast<double>(from.x);
    const auto dy = static_cast<double>(to.y) - static_cast<double>(from.y);
    const auto dz = static_cast<double>(to.z) - static_cast<double>(from.z);
    return std::hypot(dx, dy, dz);
}

using Point2i = Point2T<int>;
using Point2f = Point2T<float>;
using Point2d = Point2T<double>;

using Point3i = Point3T<int>;
using Point3f = Point3T<float>;
using Point3d = Point3T<double>;

} // namespace SZX::COMMON
