/**
 * @file UTPieChartV2Renderer.cc
 * @brief UT 饼图 V2 的 C++ 渲染辅助实现。
 */

#include "Charts/UTPieChartV2Renderer.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
constexpr qreal kPi                       = 3.14159265358979323846;
constexpr qreal kStartAngle               = -kPi / 2.0;
constexpr qreal kTwoPi                    = kPi * 2.0;
constexpr qreal kHoverLift                = 10.0;
constexpr qreal kHoverAnimationDurationMs = 300.0;
constexpr auto kHoverAnimationInterval    = std::chrono::milliseconds(16);
constexpr int kMinSegments                = 32;
constexpr int kMaxSegments                = 256;
constexpr int kDefaultColorCount          = 7;

const QColor kDefaultColors[] = {
    QColor("#FF6384"), QColor("#36A2EB"), QColor("#FFCE56"), QColor("#4BC0C0"),
    QColor("#9966FF"), QColor("#FF9F40"), QColor("#C9CBCF"),
};

QPointF polarPoint(const QPointF &center, qreal radius, qreal angle)
{
    return QPointF(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
}

int segmentCountForSlice(const UTPieChartV2Renderer::SliceGeometry &slice)
{
    const qreal sweep = (std::max)(0.0, slice.endAngle - slice.startAngle);
    return std::clamp(static_cast<int>(std::ceil(sweep * slice.outerRadius / 2.0)), kMinSegments, kMaxSegments);
}

QColor validColorOr(const QColor &color, const QColor &fallback)
{
    return color.isValid() ? color : fallback;
}

QSGGeometryNode *createFlatNode(QSGGeometry::DrawingMode mode, int vertexCount, const QColor &color)
{
    auto *node     = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), vertexCount);
    geometry->setDrawingMode(mode);

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void updateFlatColor(QSGGeometryNode *node, const QColor &color)
{
    auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
    if (material->color() == color)
        return;

    material->setColor(color);
    node->markDirty(QSGNode::DirtyMaterial);
}

void updateSliceGeometry(QSGGeometryNode *node, const UTPieChartV2Renderer::SliceGeometry &slice)
{
    const qreal sweep     = (std::max)(0.0, slice.endAngle - slice.startAngle);
    const int segments    = segmentCountForSlice(slice);
    const bool donut      = slice.innerRadius > 0.0;
    const int vertexCount = segments * 6;
    auto *geometry        = node->geometry();
    if (geometry->vertexCount() != vertexCount)
        geometry->allocate(vertexCount);

    auto *vertices = geometry->vertexDataAsPoint2D();

    for (int i = 0; i < segments; ++i)
    {
        const qreal t0       = static_cast<qreal>(i) / segments;
        const qreal t1       = static_cast<qreal>(i + 1) / segments;
        const qreal angle0   = slice.startAngle + sweep * t0;
        const qreal angle1   = slice.startAngle + sweep * t1;
        const QPointF outer0 = polarPoint(slice.center, slice.outerRadius, angle0);
        const QPointF outer1 = polarPoint(slice.center, slice.outerRadius, angle1);
        const QPointF inner0 = donut ? polarPoint(slice.center, slice.innerRadius, angle0) : slice.center;
        const QPointF inner1 = donut ? polarPoint(slice.center, slice.innerRadius, angle1) : slice.center;
        const int base       = i * 6;

        vertices[base].set(static_cast<float>(inner0.x()), static_cast<float>(inner0.y()));
        vertices[base + 1].set(static_cast<float>(outer0.x()), static_cast<float>(outer0.y()));
        vertices[base + 2].set(static_cast<float>(outer1.x()), static_cast<float>(outer1.y()));

        if (donut)
        {
            vertices[base + 3].set(static_cast<float>(inner0.x()), static_cast<float>(inner0.y()));
            vertices[base + 4].set(static_cast<float>(outer1.x()), static_cast<float>(outer1.y()));
            vertices[base + 5].set(static_cast<float>(inner1.x()), static_cast<float>(inner1.y()));
        }
        else
        {
            vertices[base + 3].set(static_cast<float>(inner0.x()), static_cast<float>(inner0.y()));
            vertices[base + 4].set(static_cast<float>(outer1.x()), static_cast<float>(outer1.y()));
            vertices[base + 5].set(static_cast<float>(inner1.x()), static_cast<float>(inner1.y()));
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
}

QSGGeometryNode *createSliceNode(const UTPieChartV2Renderer::SliceGeometry &slice, const QColor &color)
{
    auto *node =
        createFlatNode(QSGGeometry::DrawTriangles, segmentCountForSlice(slice) * 6, validColorOr(color, QColor("#3a6df0")));
    updateSliceGeometry(node, slice);
    return node;
}

bool updateSeparatorGeometry(QSGGeometryNode *node, const UTPieChartV2Renderer::SliceGeometry &slice)
{
    constexpr qreal width = 2.0;
    const qreal length    = (std::max)(0.0, slice.outerRadius - slice.innerRadius);
    if (length <= 0.0)
        return false;

    auto *vertices        = node->geometry()->vertexDataAsPoint2D();
    const qreal midRadius = slice.innerRadius + length / 2.0;
    const QPointF center  = polarPoint(slice.center, midRadius, slice.startAngle);
    const QPointF radial(std::cos(slice.startAngle), std::sin(slice.startAngle));
    const QPointF tangent(-radial.y(), radial.x());
    const QPointF halfLength = radial * (length / 2.0);
    const QPointF halfWidth  = tangent * (width / 2.0);
    const QPointF p0         = center - halfLength - halfWidth;
    const QPointF p1         = center - halfLength + halfWidth;
    const QPointF p2         = center + halfLength - halfWidth;
    const QPointF p3         = center + halfLength + halfWidth;

    vertices[0].set(static_cast<float>(p0.x()), static_cast<float>(p0.y()));
    vertices[1].set(static_cast<float>(p1.x()), static_cast<float>(p1.y()));
    vertices[2].set(static_cast<float>(p2.x()), static_cast<float>(p2.y()));
    vertices[3].set(static_cast<float>(p2.x()), static_cast<float>(p2.y()));
    vertices[4].set(static_cast<float>(p1.x()), static_cast<float>(p1.y()));
    vertices[5].set(static_cast<float>(p3.x()), static_cast<float>(p3.y()));
    node->markDirty(QSGNode::DirtyGeometry);
    return true;
}

QSGGeometryNode *createSeparatorNode(const UTPieChartV2Renderer::SliceGeometry &slice, const QColor &color)
{
    auto *node = createFlatNode(QSGGeometry::DrawTriangles, 6, validColorOr(color, QColor("#ffffff")));
    if (!updateSeparatorGeometry(node, slice))
    {
        delete node;
        return nullptr;
    }
    return node;
}
} // namespace

UTPieChartV2Renderer::UTPieChartV2Renderer(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);

    m_animationTimer.setInterval(kHoverAnimationInterval);
    connect(&m_animationTimer, &QTimer::timeout, this, &UTPieChartV2Renderer::updateHoverAnimation);
}

QVariantList UTPieChartV2Renderer::dataSeries() const
{
    return m_dataSeries;
}

void UTPieChartV2Renderer::setDataSeries(const QVariantList &dataSeries)
{
    if (m_dataSeries == dataSeries)
        return;

    m_dataSeries = dataSeries;
    rebuildData();
    emit dataSeriesChanged();
}

QVariantList UTPieChartV2Renderer::dataPoints() const
{
    return m_dataPoints;
}

void UTPieChartV2Renderer::setDataPoints(const QVariantList &dataPoints)
{
    if (m_dataPoints == dataPoints)
        return;

    m_dataPoints = dataPoints;
    rebuildData();
    emit dataPointsChanged();
}

QColor UTPieChartV2Renderer::backgroundColor() const
{
    return m_backgroundColor;
}

void UTPieChartV2Renderer::setBackgroundColor(const QColor &backgroundColor)
{
    if (m_backgroundColor == backgroundColor)
        return;

    m_backgroundColor = backgroundColor;
    invalidateGeometry();
    update();
    emit backgroundColorChanged();
}

qreal UTPieChartV2Renderer::innerRadius() const
{
    return m_innerRadius;
}

void UTPieChartV2Renderer::setInnerRadius(qreal innerRadius)
{
    const qreal bounded = (std::max)(0.0, innerRadius);
    if (qFuzzyCompare(m_innerRadius, bounded))
        return;

    m_innerRadius = bounded;
    clearHover();
    invalidateGeometry();
    notifyGeometryDataChanged();
    update();
    emit innerRadiusChanged();
}

bool UTPieChartV2Renderer::labelsVisible() const
{
    return m_labelsVisible;
}

void UTPieChartV2Renderer::setLabelsVisible(bool labelsVisible)
{
    if (m_labelsVisible == labelsVisible)
        return;

    m_labelsVisible = labelsVisible;
    invalidateGeometry();
    notifyGeometryDataChanged();
    update();
    emit labelsVisibleChanged();
}

int UTPieChartV2Renderer::hoveredIndex() const
{
    return m_hoveredIndex;
}

QVariant UTPieChartV2Renderer::hoveredDataPoint() const
{
    return pointVariantAt(m_hoveredIndex);
}

QPointF UTPieChartV2Renderer::hoverPosition() const
{
    return m_hoverPosition;
}

QVariantList UTPieChartV2Renderer::labelItems() const
{
    QVariantList labels;
    if (!m_labelsVisible || m_points.empty())
        return labels;

    const auto slices = buildSlices();
    if (slices.empty())
        return labels;

    qreal total = 0.0;
    for (const auto &point : m_points)
        total += (std::max)(0.0, point.value);
    if (total <= 0.0)
        return labels;

    labels.reserve(static_cast<qsizetype>(slices.size()));
    for (const auto &slice : slices)
    {
        if (slice.index < 0 || slice.index >= static_cast<int>(m_points.size()))
            continue;

        const qreal sweep = slice.endAngle - slice.startAngle;
        if (sweep <= 0.1)
            continue;

        const qreal midAngle = slice.startAngle + sweep / 2.0;
        const qreal labelRadius =
            slice.innerRadius > 0.0 ? (slice.innerRadius + slice.outerRadius) / 2.0 : slice.outerRadius * 0.7;
        const QPointF position = polarPoint(slice.center, labelRadius, midAngle);
        const qreal percent    = m_points[slice.index].value / total * 100.0;

        QVariantMap label;
        label.insert(QStringLiteral("x"), position.x());
        label.insert(QStringLiteral("y"), position.y());
        label.insert(QStringLiteral("text"), QString::number(percent, 'f', 2) + QStringLiteral("%"));
        label.insert(QStringLiteral("color"), QColor("#ffffff"));
        labels.push_back(label);
    }

    return labels;
}

QSGNode *UTPieChartV2Renderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData * /*data*/)
{
    updateCachedSlices();

    if (oldNode)
    {
        QSGNode *child = oldNode->firstChild();
        bool reusable  = true;
        for (const auto &slice : m_cachedSlices)
        {
            if (slice.index < 0 || slice.index >= static_cast<int>(m_points.size()) || !child || !child->nextSibling())
            {
                reusable = false;
                break;
            }

            auto *sliceNode     = static_cast<QSGGeometryNode *>(child);
            child               = child->nextSibling();
            auto *separatorNode = static_cast<QSGGeometryNode *>(child);
            child               = child->nextSibling();

            updateSliceGeometry(sliceNode, slice);
            updateFlatColor(sliceNode, validColorOr(m_points[slice.index].color, QColor("#3a6df0")));
            if (!updateSeparatorGeometry(separatorNode, slice))
            {
                reusable = false;
                break;
            }
            updateFlatColor(separatorNode, validColorOr(m_backgroundColor, QColor("#ffffff")));
        }

        if (reusable && !child)
        {
            m_geometryDirty = false;
            return oldNode;
        }
    }

    delete oldNode;
    auto *root = new QSGNode;
    for (const auto &slice : m_cachedSlices)
    {
        if (slice.index < 0 || slice.index >= static_cast<int>(m_points.size()))
            continue;

        root->appendChildNode(createSliceNode(slice, m_points[slice.index].color));
        if (auto *separator = createSeparatorNode(slice, m_backgroundColor))
            root->appendChildNode(separator);
    }

    m_geometryDirty = false;
    return root;
}

void UTPieChartV2Renderer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
    {
        clearHover();
        invalidateGeometry();
        notifyGeometryDataChanged();
        update();
    }
}

void UTPieChartV2Renderer::hoverMoveEvent(QHoverEvent *event)
{
    const int index = hitTest(event->position());
    if (index >= 0)
    {
        if (index != m_hoveredIndex)
            emit pointHovered(index, pointVariantAt(index));
        setHoveredIndex(index, event->position());
        return;
    }

    clearHover();
}

void UTPieChartV2Renderer::hoverLeaveEvent(QHoverEvent * /*event*/)
{
    clearHover();
}

void UTPieChartV2Renderer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QQuickItem::mousePressEvent(event);
        return;
    }

    const int index = hitTest(event->position());
    if (index >= 0)
    {
        setHoveredIndex(index, event->position());
        emit pointClicked(index, pointVariantAt(index));
        event->accept();
        return;
    }

    QQuickItem::mousePressEvent(event);
}

void UTPieChartV2Renderer::rebuildData()
{
    m_points.clear();
    const QVariantList points = !m_dataPoints.isEmpty() ?
        m_dataPoints :
        (!m_dataSeries.isEmpty() ? pointsFromSeriesMap(m_dataSeries.constFirst().toMap()) : QVariantList());

    m_points.reserve(points.size());
    for (int i = 0; i < points.size(); ++i)
    {
        const QVariant pointVariant = points.at(i);
        const QVariantMap pointMap  = pointVariant.toMap();
        const QColor fallback       = kDefaultColors[i % kDefaultColorCount];
        DataPoint point;
        point.label  = labelFromMap(pointMap, i);
        point.value  = valueFromMap(pointMap);
        point.color  = colorFromVariant(pointMap.value(QStringLiteral("color")), fallback);
        point.source = pointVariant;
        if (point.value > 0.0)
            m_points.push_back(point);
    }

    resetHoverOffsets();
    clearHover();
    invalidateGeometry();
    notifyGeometryDataChanged();
    update();
}

void UTPieChartV2Renderer::invalidateGeometry()
{
    m_geometryDirty     = true;
    m_cachedSlicesDirty = true;
}

void UTPieChartV2Renderer::notifyGeometryDataChanged()
{
    emit geometryDataChanged();
}

void UTPieChartV2Renderer::updateCachedSlices()
{
    if (!m_cachedSlicesDirty)
        return;

    m_cachedSlices      = buildSlices();
    m_cachedSlicesDirty = false;
}

std::vector<UTPieChartV2Renderer::SliceGeometry> UTPieChartV2Renderer::buildSlices() const
{
    std::vector<SliceGeometry> slices;
    if (m_points.empty() || width() <= 0.0 || height() <= 0.0)
        return slices;

    qreal total = 0.0;
    for (const auto &point : m_points)
        total += (std::max)(0.0, point.value);
    if (total <= 0.0)
        return slices;

    const qreal maxRadius = (std::max)(0.0, (std::min)(width(), height()) / 2.0 - kHoverLift - 2.0);
    if (maxRadius <= 0.0)
        return slices;

    qreal currentAngle = kStartAngle;
    slices.reserve(m_points.size());
    for (int i = 0; i < static_cast<int>(m_points.size()); ++i)
    {
        const qreal sweep    = (std::max)(0.0, m_points[i].value) / total * kTwoPi;
        const qreal endAngle = currentAngle + sweep;
        const QPointF center(width() / 2.0, height() / 2.0);
        const qreal offset      = i < static_cast<int>(m_sliceOffsets.size()) ? m_sliceOffsets[i] : 0.0;
        const qreal outerRadius = maxRadius + offset;

        slices.push_back({currentAngle, endAngle, (std::min)(m_innerRadius, maxRadius - 1.0), outerRadius, center, i});
        currentAngle = endAngle;
    }

    return slices;
}

int UTPieChartV2Renderer::hitTest(const QPointF &position)
{
    updateCachedSlices();

    for (const auto &slice : m_cachedSlices)
    {
        const QPointF delta  = position - slice.center;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < slice.innerRadius || distance > slice.outerRadius)
            continue;

        qreal angle = std::atan2(delta.y(), delta.x());
        if (angle < kStartAngle)
            angle += kTwoPi;

        if (angle >= slice.startAngle && angle < slice.endAngle)
            return slice.index;
    }

    return -1;
}

QVariant UTPieChartV2Renderer::pointVariantAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_points.size()))
        return QVariant();
    return m_points[index].source;
}

void UTPieChartV2Renderer::setHoveredIndex(int index, const QPointF &position)
{
    if (m_hoveredIndex == index && m_hoverPosition == position)
        return;

    const bool indexChanged = m_hoveredIndex != index;
    m_hoveredIndex          = index;
    m_hoverPosition         = position;
    if (indexChanged)
    {
        setHoverAnimationTarget(index);
        invalidateGeometry();
        notifyGeometryDataChanged();
    }
    update();
    emit hoverChanged();
}

void UTPieChartV2Renderer::clearHover()
{
    if (m_hoveredIndex == -1 && m_hoverPosition.isNull())
        return;

    m_hoveredIndex  = -1;
    m_hoverPosition = QPointF();
    setHoverAnimationTarget(-1);
    invalidateGeometry();
    notifyGeometryDataChanged();
    update();
    emit hoverChanged();
}

void UTPieChartV2Renderer::setHoverAnimationTarget(int index)
{
    if (m_sliceOffsets.size() != m_points.size())
        resetHoverOffsets();

    m_animationStartOffsets = m_sliceOffsets;
    m_animationTargetOffsets.assign(m_points.size(), 0.0);
    if (index >= 0 && index < static_cast<int>(m_animationTargetOffsets.size()))
        m_animationTargetOffsets[index] = kHoverLift;

    m_animationElapsed.restart();
    if (!m_animationTargetOffsets.empty())
        m_animationTimer.start();
}

void UTPieChartV2Renderer::updateHoverAnimation()
{
    if (!m_animationElapsed.isValid())
        return;

    const qreal progress      = (std::min)(1.0, static_cast<qreal>(m_animationElapsed.elapsed()) / kHoverAnimationDurationMs);
    const qreal easedProgress = m_hoverEasing.valueForProgress(progress);

    bool changed          = false;
    const qsizetype count = static_cast<qsizetype>((std::min)(m_animationStartOffsets.size(), m_animationTargetOffsets.size()));
    if (m_sliceOffsets.size() != static_cast<size_t>(count))
        m_sliceOffsets.resize(static_cast<size_t>(count), 0.0);

    for (qsizetype i = 0; i < count; ++i)
    {
        const qreal value = m_animationStartOffsets[static_cast<size_t>(i)] +
            (m_animationTargetOffsets[static_cast<size_t>(i)] - m_animationStartOffsets[static_cast<size_t>(i)]) * easedProgress;
        if (!qFuzzyCompare(m_sliceOffsets[static_cast<size_t>(i)] + 1.0, value + 1.0))
        {
            m_sliceOffsets[static_cast<size_t>(i)] = value;
            changed                                = true;
        }
    }

    if (progress >= 1.0)
    {
        m_sliceOffsets = m_animationTargetOffsets;
        m_animationTimer.stop();
    }

    if (changed || progress >= 1.0)
    {
        invalidateGeometry();
        update();
    }
}

void UTPieChartV2Renderer::resetHoverOffsets()
{
    m_animationTimer.stop();
    m_sliceOffsets.assign(m_points.size(), 0.0);
    m_animationStartOffsets  = m_sliceOffsets;
    m_animationTargetOffsets = m_sliceOffsets;
}

QVariantList UTPieChartV2Renderer::pointsFromSeriesMap(const QVariantMap &seriesMap)
{
    return seriesMap.value(QStringLiteral("data")).toList();
}

qreal UTPieChartV2Renderer::valueFromMap(const QVariantMap &pointMap)
{
    return pointMap.value(QStringLiteral("value")).toReal();
}

QString UTPieChartV2Renderer::labelFromMap(const QVariantMap &pointMap, int index)
{
    const QString label = pointMap.value(QStringLiteral("label")).toString();
    return label.isEmpty() ? QStringLiteral("Item %1").arg(index + 1) : label;
}

QColor UTPieChartV2Renderer::colorFromVariant(const QVariant &value, const QColor &fallback)
{
    const QColor color = value.value<QColor>();
    if (color.isValid())
        return color;

    const QColor stringColor(value.toString());
    return stringColor.isValid() ? stringColor : fallback;
}
