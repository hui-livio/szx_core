/**
 * @file UTQmlLogger.h
 * @brief QML 侧日志转发工具，输出到 Qt 日志系统。
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

class UTQmlLogger : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UTLogger)
    QML_SINGLETON
    Q_DISABLE_COPY_MOVE(UTQmlLogger)

    Q_PROPERTY(QString defaultCategory READ defaultCategory WRITE setDefaultCategory NOTIFY defaultCategoryChanged FINAL)

public:
    explicit UTQmlLogger(QObject *parent = nullptr);

    QString defaultCategory() const;
    void setDefaultCategory(const QString &category);

    Q_INVOKABLE void trace(const QVariant &message, const QString &category = {}) const;
    Q_INVOKABLE void debug(const QVariant &message, const QString &category = {}) const;
    Q_INVOKABLE void info(const QVariant &message, const QString &category = {}) const;
    Q_INVOKABLE void warn(const QVariant &message, const QString &category = {}) const;
    Q_INVOKABLE void error(const QVariant &message, const QString &category = {}) const;
    Q_INVOKABLE void critical(const QVariant &message, const QString &category = {}) const;

signals:
    void defaultCategoryChanged();

private:
    enum class Level
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    void write(Level level, const QString &category, const QVariant &message) const;

    QString m_defaultCategory = QStringLiteral("QML");
};
