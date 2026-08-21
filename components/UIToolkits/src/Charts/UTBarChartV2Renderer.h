/**
 * @file UTBarChartV2Renderer.h
 * @brief UT 柱状图 V2 的 C++ 渲染辅助声明。
 */

#pragma once

#include <QColor>
#include <QQuickItem>
#include <QRectF>
#include <QVariant>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <vector>

class UTBarChartV2Renderer : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UTBarChartV2Renderer)
    Q_DISABLE_COPY_MOVE(UTBarChartV2Renderer)

    Q_PROPERTY(QVariantList dataSeries READ dataSeries WRITE setDataSeries NOTIFY dataSeriesChanged FINAL)
    Q_PROPERTY(QVariantList dataPoints READ dataPoints WRITE setDataPoints NOTIFY dataPointsChanged FINAL)
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor NOTIFY barColorChanged FINAL)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridColorChanged FINAL)
    Q_PROPERTY(qreal maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)
    Q_PROPERTY(qreal barSpacing READ barSpacing WRITE setBarSpacing NOTIFY barSpacingChanged FINAL)
    Q_PROPERTY(qreal groupSpacing READ groupSpacing WRITE setGroupSpacing NOTIFY groupSpacingChanged FINAL)
    Q_PROPERTY(bool gridVisible READ gridVisible WRITE setGridVisible NOTIFY gridVisibleChanged FINAL)
    Q_PROPERTY(int hoveredSeriesIndex READ hoveredSeriesIndex NOTIFY hoverChanged FINAL)
    Q_PROPERTY(int hoveredDataIndex READ hoveredDataIndex NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QVariant hoveredDataPoint READ hoveredDataPoint NOTIFY hoverChanged FINAL)
    Q_PROPERTY(QRectF hoveredBarRect READ hoveredBarRect NOTIFY hoverChanged FINAL)

public:
    explicit UTBarChartV2Renderer(QQuickItem *parent = nullptr);

    QVariantList dataSeries() const;
    void setDataSeries(const QVariantList &dataSeries);

    QVariantList dataPoints() const;
    void setDataPoints(const QVariantList &dataPoints);

    QColor barColor() const;
    void setBarColor(const QColor &barColor);

    QColor gridColor() const;
    void setGridColor(const QColor &gridColor);

    qreal maxValue() const;
    void setMaxValue(qreal maxValue);

    qreal barSpacing() const;
    void setBarSpacing(qreal barSpacing);

    qreal groupSpacing() const;
    void setGroupSpacing(qreal groupSpacing);

    bool gridVisible() const;
    void setGridVisible(bool gridVisible);

    int hoveredSeriesIndex() const;
    int hoveredDataIndex() const;
    QVariant hoveredDataPoint() const;
    QRectF hoveredBarRect() const;

signals:
    void dataSeriesChanged();
    void dataPointsChanged();
    void barColorChanged();
    void gridColorChanged();
    void maxValueChanged();
    void barSpacingChanged();
    void groupSpacingChanged();
    void gridVisibleChanged();
    void hoverChanged();
    void pointClicked(int seriesIndex, int dataIndex, QVariant dataPoint);
    void pointHovered(int seriesIndex, int dataIndex, QVariant dataPoint);

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
        QVariant source;
    };

    struct DataSeries
    {
        QString name;
        QColor color;
        std::vector<DataPoint> points;
    };

    struct BarGeometry
    {
        QRectF rect;
        int seriesIndex = -1;
        int dataIndex   = -1;
    };

    void rebuildSeries();
    void invalidateGeometry();
    const std::vector<BarGeometry> &cachedBars();
    void updateCachedBars();
    std::vector<BarGeometry> buildBars() const;
    qreal effectiveMaxValue() const;
    int maxPointCount() const;
    const DataPoint *pointAt(int seriesIndex, int dataIndex) const;
    QVariant pointVariantAt(int seriesIndex, int dataIndex) const;
    void setHoveredBar(int seriesIndex, int dataIndex, const QRectF &rect);
    void clearHoveredBar();

    static QVariantList pointsFromSeriesMap(const QVariantMap &seriesMap);
    static qreal valueFromMap(const QVariantMap &pointMap);
    static QString labelFromMap(const QVariantMap &pointMap, int index);
    static QColor colorFromVariant(const QVariant &value, const QColor &fallback);

    QVariantList m_dataSeries;
    QVariantList m_dataPoints;
    QColor m_barColor      = QColor("#6d9fff");
    QColor m_gridColor     = QColor(255, 255, 255, 28);
    qreal m_maxValue       = 0.0;
    qreal m_barSpacing     = 0.2;
    qreal m_groupSpacing   = 0.3;
    bool m_gridVisible     = true;
    bool m_geometryDirty   = true;
    bool m_cachedBarsDirty = true;

    std::vector<DataSeries> m_series;
    std::vector<BarGeometry> m_cachedBars;
    int m_hoveredSeriesIndex = -1;
    int m_hoveredDataIndex   = -1;
    QRectF m_hoveredBarRect;
};
