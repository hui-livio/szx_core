/**
 * @file Geometry_test.cc
 * @brief Geometry_test.cc 覆盖的单元测试与测试辅助。
 */

#include "Geometry.hpp"
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

using namespace SZX::COMMON;

namespace
{
TEST(common_GeometryTest, PointDistanceUsesEuclideanMetric)
{
    const Point2i from{0, 0};
    const Point2i to{3, 4};
    const Point2i default2d;

    EXPECT_EQ(default2d, (Point2i{0, 0}));
    EXPECT_DOUBLE_EQ(distance(from, to), 5.0);
    EXPECT_EQ(vector_between(from, to), (Point2i{3, 4}));

    const Point3i from3d{1, 2, 3};
    const Point3i to3d{4, 6, 15};
    const Point3i default3d;
    EXPECT_EQ(default3d, (Point3i{0, 0, 0}));
    EXPECT_DOUBLE_EQ(distance(from3d, to3d), 13.0);
    EXPECT_EQ(vector_between(from3d, to3d), (Point3i{3, 4, 12}));
}

TEST(common_GeometryTest, PointVectorBetweenSaturatesIntegerOverflow)
{
    constexpr int minValue = std::numeric_limits<int>::min();
    constexpr int maxValue = std::numeric_limits<int>::max();

    EXPECT_EQ(vector_between(Point2i{minValue, maxValue}, Point2i{maxValue, minValue}), (Point2i{maxValue, minValue}));
    EXPECT_EQ(vector_between(Point3i{minValue, maxValue, minValue}, Point3i{maxValue, minValue, maxValue}),
              (Point3i{maxValue, minValue, maxValue}));
}

TEST(common_GeometryTest, UnsignedPointVectorBetweenSaturatesNegativeDeltaToZero)
{
    using Point2u = Point2T<unsigned int>;
    using Point3u = Point3T<unsigned int>;

    EXPECT_EQ(vector_between(Point2u{10U, 1U}, Point2u{5U, 7U}), (Point2u{0U, 6U}));
    EXPECT_EQ(vector_between(Point3u{10U, 1U, 9U}, Point3u{5U, 7U, 3U}), (Point3u{0U, 6U, 0U}));
}

TEST(common_GeometryTest, PointDistanceAvoidsIntegerDeltaOverflow)
{
    constexpr int minValue  = std::numeric_limits<int>::min();
    constexpr int maxValue  = std::numeric_limits<int>::max();
    const double distance2d = distance(Point2i{minValue, minValue}, Point2i{maxValue, maxValue});
    const double distance3d = distance(Point3i{minValue, minValue, minValue}, Point3i{maxValue, maxValue, maxValue});

    EXPECT_TRUE(std::isfinite(distance2d));
    EXPECT_TRUE(std::isfinite(distance3d));
    EXPECT_GT(distance2d, static_cast<double>(maxValue));
    EXPECT_GT(distance3d, static_cast<double>(maxValue));
}

TEST(common_GeometryTest, RectContainsUsesHalfOpenBounds)
{
    const Rect2i rect{10, 20, 40, 60};

    EXPECT_TRUE(rect.Contains(Point2i{10, 20}));
    EXPECT_TRUE(rect.Contains(Point2i{39, 59}));
    EXPECT_FALSE(rect.Contains(Point2i{40, 59}));
    EXPECT_FALSE(rect.Contains(Point2i{39, 60}));
}

TEST(common_GeometryTest, RectCanBeConstructedFromPointAndSize)
{
    const Rect2i rect{Point2i{10, 20}, Size2i{30, 40}};

    EXPECT_EQ(rect, (Rect2i{10, 20, 40, 60}));
    EXPECT_EQ(rect.Width(), 30);
    EXPECT_EQ(rect.Height(), 40);
}

TEST(common_GeometryTest, RectCanBeConstructedFromTwoPoints)
{
    const Rect2i rect{Point2i{10, 20}, Point2i{40, 60}};

    EXPECT_EQ(rect, (Rect2i{10, 20, 40, 60}));
    EXPECT_TRUE(rect.Contains(Point2i{39, 59}));
    EXPECT_FALSE(rect.Contains(Point2i{40, 60}));
}

TEST(common_GeometryTest, RectConstructedFromReversedTwoPointsIsEmpty)
{
    const Rect2i rect{Point2i{40, 60}, Point2i{10, 20}};

    EXPECT_TRUE(rect.IsEmpty());
    EXPECT_EQ(rect.Area(), 0);
}

TEST(common_GeometryTest, RectFromPointsNormalizesPointOrder)
{
    const auto rect = Rect2i::FromPoints(Point2i{40, 60}, Point2i{10, 20});

    EXPECT_EQ(rect, (Rect2i{10, 20, 40, 60}));
    EXPECT_EQ(rect.Area(), 1200);
}

TEST(common_GeometryTest, RectIntersectionAndUnion)
{
    const Rect2i a{0, 0, 10, 10};
    const Rect2i b{5, 6, 15, 16};

    EXPECT_EQ(a.Intersection(b), (Rect2i{5, 6, 10, 10}));
    EXPECT_EQ(a.Union(b), (Rect2i{0, 0, 15, 16}));
}

TEST(common_GeometryTest, RectEmptyTouchingAndContainmentCases)
{
    const Rect2i rect{0, 0, 10, 10};

    EXPECT_TRUE((Rect2i{0, 0, 0, 10}).IsEmpty());
    EXPECT_EQ((Rect2i{10, 10, 0, 0}).Area(), 0);
    EXPECT_FALSE(rect.Intersects(Rect2i{10, 0, 15, 5}));
    EXPECT_EQ(rect.Intersection(Rect2i{10, 0, 15, 5}), (Rect2i{0, 0, 0, 0}));
    EXPECT_TRUE(rect.Contains(Rect2i{2, 2, 5, 5}));
    EXPECT_FALSE(rect.Contains(Rect2i{9, 9, 11, 11}));
}

TEST(common_GeometryTest, RectInflateOffsetAndArea)
{
    const Rect2i rect{10, 20, 40, 60};

    EXPECT_EQ(rect.Area(), 1200);
    EXPECT_EQ(rect.Inflate(2, 3), (Rect2i{8, 17, 42, 63}));
    EXPECT_EQ(rect.Offset(-5, 6), (Rect2i{5, 26, 35, 66}));
}

TEST(common_GeometryTest, SizeAndSegmentCalculations)
{
    const Size2i size{30, 40};
    const Segment2i segment{{0, 0}, {3, 4}};

    EXPECT_FALSE(size.IsEmpty());
    EXPECT_EQ(size.Area(), 1200);
    EXPECT_TRUE((Size2i{0, 40}).IsEmpty());
    EXPECT_EQ((Size2i{-30, -40}).Area(), 0);
    EXPECT_DOUBLE_EQ(segment.Length(), 5.0);
    EXPECT_EQ(segment.LengthSquared(), 25);
}

TEST(common_GeometryTest, IntegralAreaSaturatesOnOverflow)
{
    using Size2i64                  = Size2T<std::int64_t>;
    using Rect2i64                  = RectT<std::int64_t>;
    constexpr std::int64_t maxValue = std::numeric_limits<std::int64_t>::max();

    EXPECT_EQ((Size2i64{maxValue, maxValue}).Area(), maxValue);
    EXPECT_EQ((Rect2i64{0, 0, maxValue, maxValue}).Area(), maxValue);
}

TEST(common_GeometryTest, SegmentLengthSquaredSaturatesIntegerOverflow)
{
    constexpr int minValue = std::numeric_limits<int>::min();
    constexpr int maxValue = std::numeric_limits<int>::max();
    const Segment2i segment{{minValue, minValue}, {maxValue, maxValue}};

    EXPECT_EQ(segment.LengthSquared(), std::numeric_limits<std::int64_t>::max());
}

TEST(common_GeometryTest, CircleContainsBoundary)
{
    const Circle2i circle{{0, 0}, 5};

    EXPECT_TRUE(circle.Contains(Point2i{3, 4}));
    EXPECT_TRUE(circle.Contains(Point2i{5, 0}));
    EXPECT_FALSE(circle.Contains(Point2i{6, 0}));
    EXPECT_DOUBLE_EQ((Circle2i{{0, 0}, -5}).Area(), 0.0);
}

TEST(common_GeometryTest, CircleContainsAvoidsIntegerDeltaOverflow)
{
    constexpr int minValue = std::numeric_limits<int>::min();
    constexpr int maxValue = std::numeric_limits<int>::max();
    const Circle2i circle{{minValue, minValue}, 5};

    EXPECT_FALSE(circle.Contains(Point2i{maxValue, maxValue}));
}
} // namespace
