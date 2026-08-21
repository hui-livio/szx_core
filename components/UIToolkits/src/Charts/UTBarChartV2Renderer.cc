/**
 * @file UTBarChartV2Renderer.cc
 * @brief UT 柱状图 V2 的 C++ 渲染辅助实现。
 */

#include "Charts/UTBarChartV2Renderer.h"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <cmath>

namespace
{
constexpr qreal kDefaultMaxValue = 100.0;
constexpr int kGridLineCount     = 5;

qreal boundedRatio(qreal value)
{
    return std::clamp(value, 0.0, 1.0);
}

QRectF visualBarRect(const QRectF &rect, bool hovered, const QSizeF &bounds)
{
    if (!hovered)
        return rect;

    QRectF visualRect = rect;
    visualRect.adjust(-1.0, -2.0, 1.0, 0.0);
    return visualRect.intersected(QRectF(QPointF(0.0, 0.0), bounds));
}

QColor visualBarColor(const QColor &color, bool hovered)
{
    return hovered ? color.darker(118) : color;
}

void updateRectGeometry(QSGGeometryNode *node, const QRectF &rect)
{
    auto *geometry = node->geometry();
    auto *vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(static_cast<float>(rect.left()), static_cast<float>(rect.top()));
    vertices[1].set(static_cast<float>(rect.right()), static_cast<float>(rect.top()));
    vertices[2].set(static_cast<float>(rect.left()), static_cast<float>(rect.bottom()));
    vertices[3].set(static_cast<float>(rect.right()), static_cast<float>(rect.top()));
    vertices[4].set(static_cast<float>(rect.right()), static_cast<float>(rect.bottom()));
    vertices[5].set(static_cast<float>(rect.left()), static_cast<float>(rect.bottom()));
    node->markDirty(QSGNode::DirtyGeometry);
}

void updateFlatColor(QSGGeometryNode *node, const QColor &color)
{
    auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
    if (material->color() == color)
        return;

    material->setColor(color);
    node->markDirty(QSGNode::DirtyMaterial);
}

QSGGeometryNode *createRectNode(const QRectF &rect, const QColor &color)
{
    auto *node     = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 6);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    updateRectGeometry(node, rect);
    return node;
}

QSGGeometryNode *createGridNode(const QSizeF &size, const QColor &color)
{
    auto *node     = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), (kGridLineCount + 1) * 2);
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(1.0f);

    auto *vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i <= kGridLineCount; ++i)
    {
        const auto y = static_cast<float>(size.height() * (static_cast<qreal>(i) / kGridLineCount));
        vertices[i * 2].set(0.0f, y);
        vertices[i * 2 + 1].set(static_cast<float>(size.width()), y);
    }

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}
} // namespace

UTBarChartV2Renderer::UTBarChartV2Renderer(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QVariantList UTBarChartV2Renderer::dataSeries() const
{
    return m_dataSeries;
}

void UTBarChartV2Renderer::setDataSeries(const QVariantList &dataSeries)
{
    if (m_dataSeries == dataSeries)
        return;

    m_dataSeries = dataSeries;
    rebuildSeries();
    emit dataSeriesChanged();
}

QVariantList UTBarChartV2Renderer::dataPoints() const
{
    return m_dataPoints;
}

void UTBarChartV2Renderer::setDataPoints(const QVariantList &dataPoints)
{
    if (m_dataPoints == dataPoints)
        return;

    m_dataPoints = dataPoints;
    rebuildSeries();
    emit dataPointsChanged();
}

QColor UTBarChartV2Renderer::barColor() const
{
    return m_barColor;
}

void UTBarChartV2Renderer::setBarColor(const QColor &barColor)
{
    if (m_barColor == barColor)
        return;

    m_barColor = barColor;
    rebuildSeries();
    update();
    emit barColorChanged();
}

QColor UTBarChartV2Renderer::gridColor() const
{
    return m_gridColor;
}

void UTBarChartV2Renderer::setGridColor(const QColor &gridColor)
{
    if (m_gridColor == gridColor)
        return;

    m_gridColor = gridColor;
    invalidateGeometry();
    update();
    emit gridColorChanged();
}

qreal UTBarChartV2Renderer::maxValue() const
{
    return m_maxValue;
}

void UTBarChartV2Renderer::setMaxValue(qreal maxValue)
{
    if (qFuzzyCompare(m_maxValue, maxValue))
        return;

    m_maxValue = (std::max)(0.0, maxValue);
    clearHoveredBar();
    invalidateGeometry();
    update();
    emit maxValueChanged();
}

qreal UTBarChartV2Renderer::barSpacing() const
{
    return m_barSpacing;
}

void UTBarChartV2Renderer::setBarSpacing(qreal barSpacing)
{
    const qreal bounded = boundedRatio(barSpacing);
    if (qFuzzyCompare(m_barSpacing, bounded))
        return;

    m_barSpacing = bounded;
    clearHoveredBar();
    invalidateGeometry();
    update();
    emit barSpacingChanged();
}

qreal UTBarChartV2Renderer::groupSpacing() const
{
    return m_groupSpacing;
}

void UTBarChartV2Renderer::setGroupSpacing(qreal groupSpacing)
{
    const qreal bounded = boundedRatio(groupSpacing);
    if (qFuzzyCompare(m_groupSpacing, bounded))
        return;

    m_groupSpacing = bounded;
    clearHoveredBar();
    invalidateGeometry();
    update();
    emit groupSpacingChanged();
}

bool UTBarChartV2Renderer::gridVisible() const
{
    return m_gridVisible;
}

void UTBarChartV2Renderer::setGridVisible(bool gridVisible)
{
    if (m_gridVisible == gridVisible)
        return;

    m_gridVisible = gridVisible;
    invalidateGeometry();
    update();
    emit gridVisibleChanged();
}

int UTBarChartV2Renderer::hoveredSeriesIndex() const
{
    return m_hoveredSeriesIndex;
}

int UTBarChartV2Renderer::hoveredDataIndex() const
{
    return m_hoveredDataIndex;
}

QVariant UTBarChartV2Renderer::hoveredDataPoint() const
{
    return pointVariantAt(m_hoveredSeriesIndex, m_hoveredDataIndex);
}

QRectF UTBarChartV2Renderer::hoveredBarRect() const
{
    return m_hoveredBarRect;
}

QSGNode *UTBarChartV2Renderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData * /*data*/)
{
    const QSizeF itemSize(width(), height());
    if (!oldNode || m_geometryDirty)
    {
        delete oldNode;

        auto *root = new QSGNode;
        updateCachedBars();

        if (itemSize.width() <= 0 || itemSize.height() <= 0 || m_series.empty())
        {
            m_geometryDirty = false;
            return root;
        }

        if (m_gridVisible)
            root->appendChildNode(createGridNode(itemSize, m_gridColor));

        if (m_cachedBars.empty())
        {
            m_geometryDirty = false;
            return root;
        }

        for (const auto &bar : m_cachedBars)
        {
            const bool hovered = bar.seriesIndex == m_hoveredSeriesIndex && bar.dataIndex == m_hoveredDataIndex;
            const QColor color = visualBarColor(m_series[bar.seriesIndex].color, hovered);
            const QRectF rect  = visualBarRect(bar.rect, hovered, itemSize);
            root->appendChildNode(createRectNode(rect, color));
        }

        m_geometryDirty = false;
        return root;
    }

    QSGNode *child = oldNode->firstChild();
    if (m_gridVisible && child)
        child = child->nextSibling();

    for (const auto &bar : m_cachedBars)
    {
        if (!child)
            break;

        const bool hovered = bar.seriesIndex == m_hoveredSeriesIndex && bar.dataIndex == m_hoveredDataIndex;
        const QColor color = visualBarColor(m_series[bar.seriesIndex].color, hovered);
        const QRectF rect  = visualBarRect(bar.rect, hovered, itemSize);
        auto *barNode      = static_cast<QSGGeometryNode *>(child);
        updateRectGeometry(barNode, rect);
        updateFlatColor(barNode, color);
        child = child->nextSibling();
    }

    return oldNode;
}

void UTBarChartV2Renderer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
    {
        clearHoveredBar();
        invalidateGeometry();
        update();
    }
}

void UTBarChartV2Renderer::hoverMoveEvent(QHoverEvent *event)
{
    const auto &bars = cachedBars();
    for (const auto &bar : bars)
    {
        if (bar.rect.contains(event->position()))
        {
            if (bar.seriesIndex != m_hoveredSeriesIndex || bar.dataIndex != m_hoveredDataIndex)
            {
                setHoveredBar(bar.seriesIndex, bar.dataIndex, bar.rect);
                emit pointHovered(bar.seriesIndex, bar.dataIndex, pointVariantAt(bar.seriesIndex, bar.dataIndex));
            }
            return;
        }
    }

    clearHoveredBar();
}

void UTBarChartV2Renderer::hoverLeaveEvent(QHoverEvent * /*event*/)
{
    clearHoveredBar();
}

void UTBarChartV2Renderer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QQuickItem::mousePressEvent(event);
        return;
    }

    const auto &bars = cachedBars();
    for (const auto &bar : bars)
    {
        if (bar.rect.contains(event->position()))
        {
            if (bar.seriesIndex != m_hoveredSeriesIndex || bar.dataIndex != m_hoveredDataIndex)
                setHoveredBar(bar.seriesIndex, bar.dataIndex, bar.rect);
            emit pointClicked(bar.seriesIndex, bar.dataIndex, pointVariantAt(bar.seriesIndex, bar.dataIndex));
            event->accept();
            return;
        }
    }

    if (m_hoveredSeriesIndex >= 0 && m_hoveredDataIndex >= 0)
    {
        emit pointClicked(m_hoveredSeriesIndex, m_hoveredDataIndex, pointVariantAt(m_hoveredSeriesIndex, m_hoveredDataIndex));
        event->accept();
        return;
    }

    QQuickItem::mousePressEvent(event);
}

void UTBarChartV2Renderer::rebuildSeries()
{
    m_series.clear();

    if (!m_dataPoints.isEmpty())
    {
        DataSeries singleSeries;
        singleSeries.name  = QStringLiteral("Data");
        singleSeries.color = m_barColor;
        for (int i = 0; i < m_dataPoints.size(); ++i)
        {
            const QVariant pointVariant = m_dataPoints.at(i);
            const QVariantMap pointMap  = pointVariant.toMap();
            DataPoint point;
            point.label  = labelFromMap(pointMap, i);
            point.value  = valueFromMap(pointMap);
            point.source = pointVariant;
            singleSeries.points.push_back(point);
        }
        m_series.push_back(singleSeries);
    }
    else
    {
        for (int seriesIndex = 0; seriesIndex < m_dataSeries.size(); ++seriesIndex)
        {
            const QVariantMap seriesMap = m_dataSeries.at(seriesIndex).toMap();
            DataSeries series;
            series.name  = seriesMap.value(QStringLiteral("name")).toString();
            series.color = colorFromVariant(seriesMap.value(QStringLiteral("color")), m_barColor);

            const QVariantList points = pointsFromSeriesMap(seriesMap);
            for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
            {
                const QVariant pointVariant = points.at(pointIndex);
                const QVariantMap pointMap  = pointVariant.toMap();
                DataPoint point;
                point.label  = labelFromMap(pointMap, pointIndex);
                point.value  = valueFromMap(pointMap);
                point.source = pointVariant;
                series.points.push_back(point);
            }

            if (!series.points.empty())
                m_series.push_back(series);
        }
    }

    clearHoveredBar();
    invalidateGeometry();
    update();
}

void UTBarChartV2Renderer::invalidateGeometry()
{
    m_geometryDirty   = true;
    m_cachedBarsDirty = true;
}

const std::vector<UTBarChartV2Renderer::BarGeometry> &UTBarChartV2Renderer::cachedBars()
{
    if (m_cachedBarsDirty)
        updateCachedBars();
    return m_cachedBars;
}

void UTBarChartV2Renderer::updateCachedBars()
{
    m_cachedBars      = buildBars();
    m_cachedBarsDirty = false;
}

std::vector<UTBarChartV2Renderer::BarGeometry> UTBarChartV2Renderer::buildBars() const
{
    std::vector<BarGeometry> bars;
    const int dataLength  = maxPointCount();
    const int seriesCount = static_cast<int>(m_series.size());
    const qreal max       = effectiveMaxValue();

    if (dataLength <= 0 || seriesCount <= 0 || max <= 0.0 || width() <= 0.0 || height() <= 0.0)
        return bars;

    const qreal groupTotalWidth = width() / dataLength;
    const qreal groupWidth      = groupTotalWidth * (1.0 - m_groupSpacing);
    const qreal barWidth        = (groupWidth / seriesCount) * (1.0 - (seriesCount > 1 ? m_barSpacing : 0.0));
    const qreal groupOffsetX    = (groupTotalWidth - groupWidth) / 2.0;
    const qreal barStepX        = groupWidth / seriesCount;
    const qreal barOffsetX      = (barStepX - barWidth) / 2.0;

    bars.reserve(dataLength * seriesCount);
    for (int dataIndex = 0; dataIndex < dataLength; ++dataIndex)
    {
        const qreal groupX = dataIndex * groupTotalWidth + groupOffsetX;
        for (int seriesIndex = 0; seriesIndex < seriesCount; ++seriesIndex)
        {
            if (dataIndex >= static_cast<int>(m_series[seriesIndex].points.size()))
                continue;

            const qreal value     = (std::max)(0.0, m_series[seriesIndex].points[dataIndex].value);
            const qreal ratio     = boundedRatio(value / max);
            const qreal barHeight = ratio * height();
            const qreal x         = groupX + seriesIndex * barStepX + barOffsetX;
            const qreal y         = height() - barHeight;

            if (barWidth > 0.0 && barHeight > 0.0)
                bars.push_back({QRectF(x, y, barWidth, barHeight), seriesIndex, dataIndex});
        }
    }

    return bars;
}

qreal UTBarChartV2Renderer::effectiveMaxValue() const
{
    if (m_maxValue > 0.0)
        return m_maxValue;

    qreal max = 0.0;
    for (const auto &series : m_series)
    {
        for (const auto &point : series.points)
            max = (std::max)(max, point.value);
    }

    return max > 0.0 ? max : kDefaultMaxValue;
}

int UTBarChartV2Renderer::maxPointCount() const
{
    int count = 0;
    for (const auto &series : m_series)
        count = (std::max)(count, static_cast<int>(series.points.size()));
    return count;
}

const UTBarChartV2Renderer::DataPoint *UTBarChartV2Renderer::pointAt(int seriesIndex, int dataIndex) const
{
    if (seriesIndex < 0 || seriesIndex >= static_cast<int>(m_series.size()))
        return nullptr;

    const auto &points = m_series[seriesIndex].points;
    if (dataIndex < 0 || dataIndex >= static_cast<int>(points.size()))
        return nullptr;

    return &points[dataIndex];
}

QVariant UTBarChartV2Renderer::pointVariantAt(int seriesIndex, int dataIndex) const
{
    const DataPoint *point = pointAt(seriesIndex, dataIndex);
    return point ? point->source : QVariant();
}

void UTBarChartV2Renderer::setHoveredBar(int seriesIndex, int dataIndex, const QRectF &rect)
{
    if (m_hoveredSeriesIndex == seriesIndex && m_hoveredDataIndex == dataIndex && m_hoveredBarRect == rect)
        return;

    m_hoveredSeriesIndex = seriesIndex;
    m_hoveredDataIndex   = dataIndex;
    m_hoveredBarRect     = rect;
    update();
    emit hoverChanged();
}

void UTBarChartV2Renderer::clearHoveredBar()
{
    if (m_hoveredSeriesIndex == -1 && m_hoveredDataIndex == -1 && m_hoveredBarRect.isNull())
        return;

    m_hoveredSeriesIndex = -1;
    m_hoveredDataIndex   = -1;
    m_hoveredBarRect     = QRectF();
    update();
    emit hoverChanged();
}

QVariantList UTBarChartV2Renderer::pointsFromSeriesMap(const QVariantMap &seriesMap)
{
    return seriesMap.value(QStringLiteral("data")).toList();
}

qreal UTBarChartV2Renderer::valueFromMap(const QVariantMap &pointMap)
{
    return pointMap.value(QStringLiteral("value")).toReal();
}

QString UTBarChartV2Renderer::labelFromMap(const QVariantMap &pointMap, int index)
{
    const QString label = pointMap.value(QStringLiteral("label")).toString();
    return label.isEmpty() ? QStringLiteral("Item %1").arg(index + 1) : label;
}

QColor UTBarChartV2Renderer::colorFromVariant(const QVariant &value, const QColor &fallback)
{
    const QColor color = value.value<QColor>();
    if (color.isValid())
        return color;

    const QColor stringColor(value.toString());
    return stringColor.isValid() ? stringColor : fallback;
}
