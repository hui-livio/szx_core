/**
 * @file UTAreaChartV2Renderer.h
 * @brief UT 面积图 V2 的 C++ 渲染辅助声明。
 */

#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QVariant>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <vector>

class UTAreaChartV2Renderer : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UTAreaChartV2Renderer)
    Q_DISABLE_COPY_MOVE(UTAreaChartV2Renderer)

    Q_PROPERTY(QVariantList dataSeries READ dataSeries WRITE setDataSeries NOTIFY dataSeriesChanged FINAL)
    Q_PROPERTY(QVariantList dataPoints READ dataPoints WRITE setDataPoints NOTIFY dataPointsChanged FINAL)
    Q_PROPERTY(int lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged FINAL)
    Q_PROPERTY(int hoveredIndex READ hoveredIndex NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QVariant hoveredDataPoint READ hoveredDataPoint NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QPointF hoveredPoint READ hoveredPoint NOTIFY hoverChanged FINAL)

public:
    enum LineStyle
    {
        Smooth = 0,
        Linear = 1,
        Step   = 2,
    };
    Q_ENUM(LineStyle)

    explicit UTAreaChartV2Renderer(QQuickItem *parent = nullptr);

    QVariantList dataSeries() const;
    void setDataSeries(const QVariantList &dataSeries);

    QVariantList dataPoints() const;
    void setDataPoints(const QVariantList &dataPoints);

    int lineStyle() const;
    void setLineStyle(int lineStyle);

    int hoveredIndex() const;
    QVariant hoveredDataPoint() const;
    QPointF hoveredPoint() const;

signals:
    void dataSeriesChanged();
    void dataPointsChanged();
    void lineStyleChanged();
    void hoverChanged();
    void pointClicked(int index, QVariant dataPoint);
    void pointHovered(int index, QVariant dataPoint);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct DataPoint
    {
        QString label;
        QString month;
        qreal value = 0.0;
        QVariant source;
    };

    struct DataSeries
    {
        QString name;
        QColor color;
        std::vector<DataPoint> points;
    };

    void rebuildSeries();
    void invalidateGeometry();
    void updateCachedPoints();
    std::vector<std::vector<QPointF>> buildSeriesPoints() const;
    std::vector<QPointF> expandedPath(const std::vector<QPointF> &points) const;
    int hitTest(const QPointF &position);
    qreal effectiveMaxValue() const;
    int maxPointCount() const;
    QVariant pointVariantAt(int index) const;
    void setHoveredIndex(int index);
    void clearHover();

    static QVariantList pointsFromSeriesMap(const QVariantMap &seriesMap);
    static qreal valueFromMap(const QVariantMap &pointMap);
    static QString labelFromMap(const QVariantMap &pointMap, int index);
    static QString monthFromMap(const QVariantMap &pointMap, int index);
    static QColor colorFromVariant(const QVariant &value, const QColor &fallback);

    QVariantList m_dataSeries;
    QVariantList m_dataPoints;
    int m_lineStyle          = Smooth;
    bool m_geometryDirty     = true;
    bool m_cachedPointsDirty = true;

    std::vector<DataSeries> m_series;
    std::vector<std::vector<QPointF>> m_cachedPoints;
    int m_hoveredIndex = -1;
    QPointF m_hoveredPoint;
};
