/**
 * @file UTQmlLogger.cc
 * @brief QML 日志转发工具实现。
 */

#include "UTQmlLogger.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QtLogging>

namespace
{

QString variantToString(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return {};

    if (value.metaType().id() == QMetaType::QString)
        return value.toString();
    if (value.metaType().id() == QMetaType::QByteArray)
        return QString::fromUtf8(value.toByteArray());

    const QJsonValue jsonValue = QJsonValue::fromVariant(value);
    if (jsonValue.isObject())
        return QString::fromUtf8(QJsonDocument(jsonValue.toObject()).toJson(QJsonDocument::Compact));
    if (jsonValue.isArray())
        return QString::fromUtf8(QJsonDocument(jsonValue.toArray()).toJson(QJsonDocument::Compact));
    if (jsonValue.isString())
        return jsonValue.toString();
    if (jsonValue.isBool())
        return jsonValue.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (jsonValue.isDouble())
        return QString::number(jsonValue.toDouble(), 'g', 15);

    return value.toString();
}

QByteArray normalizedCategory(const QString &category, const QString &fallback)
{
    const QString fallbackCategory = fallback.trimmed().isEmpty() ? QStringLiteral("QML") : fallback.trimmed();
    const QString categoryName     = category.trimmed();
    if (categoryName.isEmpty() || categoryName == fallbackCategory)
        return fallbackCategory.toUtf8();
    if (categoryName.startsWith(fallbackCategory + QStringLiteral("|")))
        return categoryName.toUtf8();

    return (fallbackCategory + QStringLiteral("|") + categoryName).toUtf8();
}

} // namespace

UTQmlLogger::UTQmlLogger(QObject *parent) :
    QObject(parent)
{
}

QString UTQmlLogger::defaultCategory() const
{
    return m_defaultCategory;
}

void UTQmlLogger::setDefaultCategory(const QString &category)
{
    const QString normalized = category.trimmed().isEmpty() ? QStringLiteral("QML") : category.trimmed();
    if (m_defaultCategory == normalized)
        return;

    m_defaultCategory = normalized;
    emit defaultCategoryChanged();
}

void UTQmlLogger::trace(const QVariant &message, const QString &category) const
{
    write(Level::Trace, category, message);
}

void UTQmlLogger::debug(const QVariant &message, const QString &category) const
{
    write(Level::Debug, category, message);
}

void UTQmlLogger::info(const QVariant &message, const QString &category) const
{
    write(Level::Info, category, message);
}

void UTQmlLogger::warn(const QVariant &message, const QString &category) const
{
    write(Level::Warn, category, message);
}

void UTQmlLogger::error(const QVariant &message, const QString &category) const
{
    write(Level::Error, category, message);
}

void UTQmlLogger::critical(const QVariant &message, const QString &category) const
{
    write(Level::Critical, category, message);
}

void UTQmlLogger::write(Level level, const QString &category, const QVariant &message) const
{
    const QByteArray categoryUtf8 = normalizedCategory(category, m_defaultCategory);
    const QByteArray messageUtf8  = variantToString(message).toUtf8();
    QMessageLogger logger(nullptr, 0, nullptr, categoryUtf8.constData());

    switch (level)
    {
    case Level::Trace:
        logger.debug("[TRACE] %s", messageUtf8.constData());
        break;
    case Level::Debug:
        logger.debug("%s", messageUtf8.constData());
        break;
    case Level::Info:
        logger.info("%s", messageUtf8.constData());
        break;
    case Level::Warn:
        logger.warning("%s", messageUtf8.constData());
        break;
    case Level::Error:
        logger.critical("[ERROR] %s", messageUtf8.constData());
        break;
    case Level::Critical:
        logger.critical("[CRITICAL] %s", messageUtf8.constData());
        break;
    }
}
