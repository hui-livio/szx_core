#include <gtest/gtest.h>

#include "Geometry.hpp"

#include <cmath>
#include <limits>

using namespace SZX::COMMON;

TEST(CommonGeometryTest, MeasuresPointsAndShapes)
{
    EXPECT_DOUBLE_EQ(distance(Point2i{0, 0}, Point2i{3, 4}), 5.0);
    EXPECT_EQ(vector_between(Point2i{0, 0}, Point2i{3, 4}), (Point2i{3, 4}));

    const Rect2i rect{10, 20, 40, 60};
    EXPECT_TRUE(rect.Contains(Point2i{10, 20}));
    EXPECT_FALSE(rect.Contains(Point2i{40, 59}));
    EXPECT_EQ(rect.Area(), 1200);
    EXPECT_EQ(rect.Inflate(2, 3), (Rect2i{8, 17, 42, 63}));
    EXPECT_EQ(rect.Offset(-5, 6), (Rect2i{5, 26, 35, 66}));

    const Circle2i circle{{0, 0}, 5};
    EXPECT_TRUE(circle.Contains(Point2i{3, 4}));
    EXPECT_FALSE(circle.Contains(Point2i{6, 0}));
}

TEST(CommonGeometryTest, HandlesIntersectionsAndOverflowSafely)
{
    const Rect2i a{0, 0, 10, 10};
    const Rect2i b{5, 6, 15, 16};
    EXPECT_EQ(a.Intersection(b), (Rect2i{5, 6, 10, 10}));
    EXPECT_EQ(a.Union(b), (Rect2i{0, 0, 15, 16}));
    EXPECT_FALSE(a.Intersects(Rect2i{10, 0, 15, 5}));

    constexpr auto maxValue = std::numeric_limits<std::int64_t>::max();
    EXPECT_EQ((Size2T<std::int64_t>{maxValue, maxValue}).Area(), maxValue);
    const Segment2i segment{{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()},
                            {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}};
    EXPECT_EQ(segment.LengthSquared(), maxValue);
    EXPECT_TRUE(
        std::isfinite(distance(Point2i{std::numeric_limits<int>::min(), 0}, Point2i{std::numeric_limits<int>::max(), 0})));
}
