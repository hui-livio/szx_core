/**
 * @file UTPieChartV2Renderer.h
 * @brief UT 饼图 V2 的 C++ 渲染辅助声明。
 */

#pragma once

#include <QColor>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QPointF>
#include <QQuickItem>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <vector>

class UTPieChartV2Renderer : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UTPieChartV2Renderer)
    Q_DISABLE_COPY_MOVE(UTPieChartV2Renderer)

    Q_PROPERTY(QVariantList dataSeries READ dataSeries WRITE setDataSeries NOTIFY dataSeriesChanged FINAL)
    Q_PROPERTY(QVariantList dataPoints READ dataPoints WRITE setDataPoints NOTIFY dataPointsChanged FINAL)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged FINAL)
    Q_PROPERTY(qreal innerRadius READ innerRadius WRITE setInnerRadius NOTIFY innerRadiusChanged FINAL)
    Q_PROPERTY(bool labelsVisible READ labelsVisible WRITE setLabelsVisible NOTIFY labelsVisibleChanged FINAL)
    Q_PROPERTY(int hoveredIndex READ hoveredIndex NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QVariant hoveredDataPoint READ hoveredDataPoint NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QPointF hoverPosition READ hoverPosition NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QVariantList labelItems READ labelItems NOTIFY geometryDataChanged FINAL)

public:
    explicit UTPieChartV2Renderer(QQuickItem *parent = nullptr);

    QVariantList dataSeries() const;
    void setDataSeries(const QVariantList &dataSeries);

    QVariantList dataPoints() const;
    void setDataPoints(const QVariantList &dataPoints);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &backgroundColor);

    qreal innerRadius() const;
    void setInnerRadius(qreal innerRadius);

    bool labelsVisible() const;
    void setLabelsVisible(bool labelsVisible);

    int hoveredIndex() const;
    QVariant hoveredDataPoint() const;
    QPointF hoverPosition() const;
    QVariantList labelItems() const;

signals:
    void dataSeriesChanged();
    void dataPointsChanged();
    void backgroundColorChanged();
    void innerRadiusChanged();
    void labelsVisibleChanged();
    void hoverChanged();
    void geometryDataChanged();
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
        qreal value = 0.0;
        QColor color;
        QVariant source;
    };

public:
    struct SliceGeometry
    {
        qreal startAngle  = 0.0;
        qreal endAngle    = 0.0;
        qreal innerRadius = 0.0;
        qreal outerRadius = 0.0;
        QPointF center;
        int index = -1;
    };

private:
    void rebuildData();
    void invalidateGeometry();
    void notifyGeometryDataChanged();
    void updateCachedSlices();
    std::vector<SliceGeometry> buildSlices() const;
    int hitTest(const QPointF &position);
    QVariant pointVariantAt(int index) const;
    void setHoveredIndex(int index, const QPointF &position);
    void clearHover();
    void setHoverAnimationTarget(int index);
    void updateHoverAnimation();
    void resetHoverOffsets();

    static QVariantList pointsFromSeriesMap(const QVariantMap &seriesMap);
    static qreal valueFromMap(const QVariantMap &pointMap);
    static QString labelFromMap(const QVariantMap &pointMap, int index);
    static QColor colorFromVariant(const QVariant &value, const QColor &fallback);

    QVariantList m_dataSeries;
    QVariantList m_dataPoints;
    QColor m_backgroundColor = QColor("#ffffff");
    qreal m_innerRadius      = 0.0;
    bool m_labelsVisible     = true;
    bool m_geometryDirty     = true;
    bool m_cachedSlicesDirty = true;

    std::vector<DataPoint> m_points;
    std::vector<SliceGeometry> m_cachedSlices;
    std::vector<qreal> m_sliceOffsets;
    std::vector<qreal> m_animationStartOffsets;
    std::vector<qreal> m_animationTargetOffsets;
    int m_hoveredIndex = -1;
    QPointF m_hoverPosition;
    QTimer m_animationTimer;
    QElapsedTimer m_animationElapsed;
    QEasingCurve m_hoverEasing = QEasingCurve(QEasingCurve::OutCubic);
};
