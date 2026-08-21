/**
 * @file UTAreaChartV2Renderer.cc
 * @brief UT 面积图 V2 的 C++ 渲染辅助实现。
 */

#include "Charts/UTAreaChartV2Renderer.h"

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
constexpr qreal kPi              = 3.14159265358979323846;
constexpr int kDefaultColorCount = 5;

const QColor kDefaultColors[] = {
    QColor("#3a6df0"), QColor("#f39c12"), QColor("#2ecc71"), QColor("#e74c3c"), QColor("#9b59b6"),
};

QColor areaColorFor(const QColor &color)
{
    QColor result = color;
    result.setAlphaF(0.30F);
    return result;
}

QSGGeometryNode *createStrokeNode(const std::vector<QPointF> &points, const QColor &color, qreal width)
{
    if (points.size() < 2)
        return nullptr;

    const int pointCount = static_cast<int>(points.size());
    auto *node           = new QSGGeometryNode;
    auto *geometry       = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), pointCount * 2);
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);

    auto *vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < pointCount; ++i)
    {
        QPointF direction;
        if (i == 0)
            direction = points[1] - points[0];
        else if (i == pointCount - 1)
            direction = points[i] - points[i - 1];
        else
            direction = points[i + 1] - points[i - 1];

        const qreal length = std::hypot(direction.x(), direction.y());
        direction          = length > 0.0 ? direction / length : QPointF(1.0, 0.0);

        const QPointF normal(-direction.y() * width / 2.0, direction.x() * width / 2.0);
        const QPointF top    = points[i] + normal;
        const QPointF bottom = points[i] - normal;
        vertices[i * 2].set(static_cast<float>(top.x()), static_cast<float>(top.y()));
        vertices[i * 2 + 1].set(static_cast<float>(bottom.x()), static_cast<float>(bottom.y()));
    }

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode *createAreaNode(const std::vector<QPointF> &path, qreal baseline, const QColor &color)
{
    auto *node     = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(path.size() * 2));
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);

    auto *vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < static_cast<int>(path.size()); ++i)
    {
        vertices[i * 2].set(static_cast<float>(path[i].x()), static_cast<float>(path[i].y()));
        vertices[i * 2 + 1].set(static_cast<float>(path[i].x()), static_cast<float>(baseline));
    }

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode *createPointNode(const QPointF &center, qreal radius, const QColor &color)
{
    constexpr int segments = 48;
    auto *node             = new QSGGeometryNode;
    auto *geometry         = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), segments * 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    auto *vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < segments; ++i)
    {
        const qreal angle0 = static_cast<qreal>(i) / segments * kPi * 2.0;
        const qreal angle1 = static_cast<qreal>(i + 1) / segments * kPi * 2.0;
        const int base     = i * 3;

        vertices[base].set(static_cast<float>(center.x()), static_cast<float>(center.y()));
        vertices[base + 1].set(static_cast<float>(center.x() + std::cos(angle0) * radius),
                               static_cast<float>(center.y() + std::sin(angle0) * radius));
        vertices[base + 2].set(static_cast<float>(center.x() + std::cos(angle1) * radius),
                               static_cast<float>(center.y() + std::sin(angle1) * radius));
    }

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);

    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QPointF cubicPoint(const QPointF &p0, const QPointF &c1, const QPointF &c2, const QPointF &p1, qreal t)
{
    const qreal inv = 1.0 - t;
    const qreal a   = inv * inv * inv;
    const qreal b   = 3.0 * inv * inv * t;
    const qreal c   = 3.0 * inv * t * t;
    const qreal d   = t * t * t;
    return QPointF(a * p0.x() + b * c1.x() + c * c2.x() + d * p1.x(), a * p0.y() + b * c1.y() + c * c2.y() + d * p1.y());
}

QPointF quadraticPoint(const QPointF &p0, const QPointF &control, const QPointF &p1, qreal t)
{
    const qreal inv = 1.0 - t;
    const qreal a   = inv * inv;
    const qreal b   = 2.0 * inv * t;
    const qreal c   = t * t;
    return QPointF(a * p0.x() + b * control.x() + c * p1.x(), a * p0.y() + b * control.y() + c * p1.y());
}

void appendCubic(std::vector<QPointF> &path, const QPointF &p0, const QPointF &c1, const QPointF &c2, const QPointF &p1)
{
    constexpr int samples = 48;
    for (int sample = 1; sample <= samples; ++sample)
        path.push_back(cubicPoint(p0, c1, c2, p1, static_cast<qreal>(sample) / samples));
}

void appendQuadratic(std::vector<QPointF> &path, const QPointF &p0, const QPointF &control, const QPointF &p1)
{
    constexpr int samples = 48;
    for (int sample = 1; sample <= samples; ++sample)
        path.push_back(quadraticPoint(p0, control, p1, static_cast<qreal>(sample) / samples));
}
} // namespace

UTAreaChartV2Renderer::UTAreaChartV2Renderer(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QVariantList UTAreaChartV2Renderer::dataSeries() const
{
    return m_dataSeries;
}

void UTAreaChartV2Renderer::setDataSeries(const QVariantList &dataSeries)
{
    if (m_dataSeries == dataSeries)
        return;

    m_dataSeries = dataSeries;
    rebuildSeries();
    emit dataSeriesChanged();
}

QVariantList UTAreaChartV2Renderer::dataPoints() const
{
    return m_dataPoints;
}

void UTAreaChartV2Renderer::setDataPoints(const QVariantList &dataPoints)
{
    if (m_dataPoints == dataPoints)
        return;

    m_dataPoints = dataPoints;
    rebuildSeries();
    emit dataPointsChanged();
}

int UTAreaChartV2Renderer::lineStyle() const
{
    return m_lineStyle;
}

void UTAreaChartV2Renderer::setLineStyle(int lineStyle)
{
    const int bounded = std::clamp(lineStyle, static_cast<int>(Smooth), static_cast<int>(Step));
    if (m_lineStyle == bounded)
        return;

    m_lineStyle = bounded;
    invalidateGeometry();
    update();
    emit lineStyleChanged();
}

int UTAreaChartV2Renderer::hoveredIndex() const
{
    return m_hoveredIndex;
}

QVariant UTAreaChartV2Renderer::hoveredDataPoint() const
{
    return pointVariantAt(m_hoveredIndex);
}

QPointF UTAreaChartV2Renderer::hoveredPoint() const
{
    return m_hoveredPoint;
}

QSGNode *UTAreaChartV2Renderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData * /*data*/)
{
    updateCachedPoints();

    auto appendHoverNodes = [this](QSGNode *root)
    {
        if (m_hoveredIndex < 0)
            return;

        for (int seriesIndex = 0; seriesIndex < static_cast<int>(m_cachedPoints.size()); ++seriesIndex)
        {
            if (m_hoveredIndex >= static_cast<int>(m_cachedPoints[seriesIndex].size()))
                continue;
            root->appendChildNode(createPointNode(m_cachedPoints[seriesIndex][m_hoveredIndex], 6.0, m_series[seriesIndex].color));
            root->appendChildNode(createPointNode(m_cachedPoints[seriesIndex][m_hoveredIndex], 3.2, QColor("#ffffff")));
        }
    };

    if (oldNode && !m_geometryDirty)
    {
        int staticNodeCount = 0;
        for (int seriesIndex = 0; seriesIndex < static_cast<int>(m_cachedPoints.size()); ++seriesIndex)
        {
            if (m_cachedPoints[seriesIndex].size() >= 2)
                staticNodeCount += 2;
        }

        QSGNode *child = oldNode->firstChild();
        for (int i = 0; i < staticNodeCount && child; ++i)
            child = child->nextSibling();

        while (child)
        {
            QSGNode *next = child->nextSibling();
            oldNode->removeChildNode(child);
            delete child;
            child = next;
        }

        appendHoverNodes(oldNode);
        return oldNode;
    }

    delete oldNode;
    auto *root           = new QSGNode;
    const qreal baseline = height() - 8.0;
    for (int seriesIndex = 0; seriesIndex < static_cast<int>(m_cachedPoints.size()); ++seriesIndex)
    {
        const auto path = expandedPath(m_cachedPoints[seriesIndex]);
        if (path.size() < 2)
            continue;

        const QColor color = m_series[seriesIndex].color;
        root->appendChildNode(createAreaNode(path, baseline, areaColorFor(color)));
        if (auto *strokeNode = createStrokeNode(path, color, 2.0))
            root->appendChildNode(strokeNode);
    }

    appendHoverNodes(root);

    m_geometryDirty = false;
    return root;
}

void UTAreaChartV2Renderer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
    {
        clearHover();
        invalidateGeometry();
        update();
    }
}

void UTAreaChartV2Renderer::hoverMoveEvent(QHoverEvent *event)
{
    const int index = hitTest(event->position());
    if (index >= 0)
    {
        if (index != m_hoveredIndex)
            emit pointHovered(index, pointVariantAt(index));
        setHoveredIndex(index);
        return;
    }

    clearHover();
}

void UTAreaChartV2Renderer::hoverLeaveEvent(QHoverEvent * /*event*/)
{
    clearHover();
}

void UTAreaChartV2Renderer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QQuickItem::mousePressEvent(event);
        return;
    }

    const int index = hitTest(event->position());
    if (index >= 0)
    {
        setHoveredIndex(index);
        emit pointClicked(index, pointVariantAt(index));
        event->accept();
        return;
    }

    QQuickItem::mousePressEvent(event);
}

void UTAreaChartV2Renderer::rebuildSeries()
{
    m_series.clear();

    if (!m_dataPoints.isEmpty())
    {
        DataSeries series;
        series.name  = QStringLiteral("Data");
        series.color = kDefaultColors[0];
        for (int i = 0; i < m_dataPoints.size(); ++i)
        {
            const QVariant pointVariant = m_dataPoints.at(i);
            const QVariantMap pointMap  = pointVariant.toMap();
            series.points.push_back({labelFromMap(pointMap, i), monthFromMap(pointMap, i), valueFromMap(pointMap), pointVariant});
        }
        if (!series.points.empty())
            m_series.push_back(series);
    }
    else
    {
        for (int seriesIndex = 0; seriesIndex < m_dataSeries.size(); ++seriesIndex)
        {
            const QVariantMap seriesMap = m_dataSeries.at(seriesIndex).toMap();
            DataSeries series;
            series.name = seriesMap.value(QStringLiteral("name")).toString();
            series.color =
                colorFromVariant(seriesMap.value(QStringLiteral("color")), kDefaultColors[seriesIndex % kDefaultColorCount]);

            const QVariantList points = pointsFromSeriesMap(seriesMap);
            for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
            {
                const QVariant pointVariant = points.at(pointIndex);
                const QVariantMap pointMap  = pointVariant.toMap();
                series.points.push_back({labelFromMap(pointMap, pointIndex), monthFromMap(pointMap, pointIndex),
                                         valueFromMap(pointMap), pointVariant});
            }

            if (!series.points.empty())
                m_series.push_back(series);
        }
    }

    clearHover();
    invalidateGeometry();
    update();
}

void UTAreaChartV2Renderer::invalidateGeometry()
{
    m_geometryDirty     = true;
    m_cachedPointsDirty = true;
}

void UTAreaChartV2Renderer::updateCachedPoints()
{
    if (!m_cachedPointsDirty)
        return;

    m_cachedPoints      = buildSeriesPoints();
    m_cachedPointsDirty = false;
}

std::vector<std::vector<QPointF>> UTAreaChartV2Renderer::buildSeriesPoints() const
{
    std::vector<std::vector<QPointF>> result;
    const int count = maxPointCount();
    const qreal max = effectiveMaxValue();
    if (count <= 0 || max <= 0.0 || width() <= 0.0 || height() <= 0.0)
        return result;

    const qreal top         = 8.0;
    const qreal chartHeight = (std::max)(1.0, height() - 16.0);
    const qreal step        = width() / count;
    result.reserve(m_series.size());

    for (const auto &series : m_series)
    {
        std::vector<QPointF> points;
        points.reserve(series.points.size());
        for (int i = 0; i < static_cast<int>(series.points.size()); ++i)
        {
            const qreal value = (std::max)(0.0, series.points[i].value);
            const qreal x     = step * i + step / 2.0;
            const qreal y     = top + chartHeight - std::clamp(value / max, 0.0, 1.0) * chartHeight;
            points.push_back(QPointF(x, y));
        }
        result.push_back(points);
    }

    return result;
}

std::vector<QPointF> UTAreaChartV2Renderer::expandedPath(const std::vector<QPointF> &points) const
{
    if (points.size() < 2 || m_lineStyle == Linear)
        return points;

    std::vector<QPointF> path;
    if (m_lineStyle == Step)
    {
        path.reserve(points.size() * 2);
        path.push_back(points.front());
        for (int i = 1; i < static_cast<int>(points.size()); ++i)
        {
            path.push_back(QPointF(points[i].x(), points[i - 1].y()));
            path.push_back(points[i]);
        }
        return path;
    }

    constexpr int samplesPerSegment = 48;
    path.reserve((points.size() - 1) * samplesPerSegment + 1);
    path.push_back(points.front());

    if (points.size() == 2)
    {
        const QPointF control((points[0].x() + points[1].x()) / 2.0, points[0].y());
        appendQuadratic(path, points[0], control, points[1]);
        return path;
    }

    const QPointF firstControl1(points[0].x() + (points[1].x() - points[0].x()) * 0.25, points[0].y());
    const QPointF firstControl2(points[1].x() - (points[2].x() - points[0].x()) * 0.25,
                                points[1].y() - (points[2].y() - points[0].y()) * 0.25);
    appendCubic(path, points[0], firstControl1, firstControl2, points[1]);

    for (int i = 2; i < static_cast<int>(points.size()) - 1; ++i)
    {
        const QPointF p0 = points[i - 2];
        const QPointF p1 = points[i - 1];
        const QPointF p2 = points[i];
        const QPointF p3 = points[i + 1];
        const QPointF control1(p1.x() + (p2.x() - p0.x()) / 6.0, p1.y() + (p2.y() - p0.y()) / 6.0);
        const QPointF control2(p2.x() - (p3.x() - p1.x()) / 6.0, p2.y() - (p3.y() - p1.y()) / 6.0);
        appendCubic(path, p1, control1, control2, p2);
    }

    const int last = static_cast<int>(points.size()) - 1;
    if (last >= 2)
    {
        const QPointF control1(points[last - 1].x() + (points[last].x() - points[last - 2].x()) * 0.25,
                               points[last - 1].y() + (points[last].y() - points[last - 2].y()) * 0.25);
        const QPointF control2(points[last].x() - (points[last].x() - points[last - 1].x()) * 0.25, points[last].y());
        appendCubic(path, points[last - 1], control1, control2, points[last]);
    }

    return path;
}

int UTAreaChartV2Renderer::hitTest(const QPointF &position)
{
    updateCachedPoints();
    const int count = maxPointCount();
    if (count <= 0 || width() <= 0.0)
        return -1;

    const qreal step = width() / count;
    const int index  = std::clamp(static_cast<int>(position.x() / step), 0, count - 1);
    return index;
}

qreal UTAreaChartV2Renderer::effectiveMaxValue() const
{
    qreal max = 0.0;
    for (const auto &series : m_series)
    {
        for (const auto &point : series.points)
            max = (std::max)(max, point.value);
    }
    return max > 0.0 ? max : 100.0;
}

int UTAreaChartV2Renderer::maxPointCount() const
{
    int count = 0;
    for (const auto &series : m_series)
        count = (std::max)(count, static_cast<int>(series.points.size()));
    return count;
}

QVariant UTAreaChartV2Renderer::pointVariantAt(int index) const
{
    if (m_series.empty() || index < 0 || index >= static_cast<int>(m_series.front().points.size()))
        return QVariant();
    return m_series.front().points[index].source;
}

void UTAreaChartV2Renderer::setHoveredIndex(int index)
{
    if (m_hoveredIndex == index)
        return;

    m_hoveredIndex = index;
    if (!m_cachedPoints.empty() && index >= 0 && index < static_cast<int>(m_cachedPoints.front().size()))
        m_hoveredPoint = m_cachedPoints.front()[index];
    else
        m_hoveredPoint = QPointF();
    update();
    emit hoverChanged();
}

void UTAreaChartV2Renderer::clearHover()
{
    if (m_hoveredIndex == -1 && m_hoveredPoint.isNull())
        return;

    m_hoveredIndex = -1;
    m_hoveredPoint = QPointF();
    update();
    emit hoverChanged();
}

QVariantList UTAreaChartV2Renderer::pointsFromSeriesMap(const QVariantMap &seriesMap)
{
    return seriesMap.value(QStringLiteral("data")).toList();
}

qreal UTAreaChartV2Renderer::valueFromMap(const QVariantMap &pointMap)
{
    return pointMap.value(QStringLiteral("value")).toReal();
}

QString UTAreaChartV2Renderer::labelFromMap(const QVariantMap &pointMap, int index)
{
    const QString label = pointMap.value(QStringLiteral("label")).toString();
    return label.isEmpty() ? monthFromMap(pointMap, index) : label;
}

QString UTAreaChartV2Renderer::monthFromMap(const QVariantMap &pointMap, int index)
{
    const QString month = pointMap.value(QStringLiteral("month")).toString();
    return month.isEmpty() ? QStringLiteral("Item %1").arg(index + 1) : month;
}

QColor UTAreaChartV2Renderer::colorFromVariant(const QVariant &value, const QColor &fallback)
{
    const QColor color = value.value<QColor>();
    if (color.isValid())
        return color;

    const QColor stringColor(value.toString());
    return stringColor.isValid() ? stringColor : fallback;
}
