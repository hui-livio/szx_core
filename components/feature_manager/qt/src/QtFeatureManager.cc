/**
 * @file QtFeatureManager.cc
 * @brief FeatureManager 的 Qt/QML 适配实现。
 */

#include "QtFeatureManager.h"

#include "FeatureManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

using namespace SZX::FEATURE;

namespace
{
using namespace std::chrono_literals;
constexpr auto kReloadDebounce = 100ms;

QVariant toQVariant(const FeatureValue &value)
{
    return std::visit(
        [](const auto &item) -> QVariant
        {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<T, std::string>)
                return QString::fromStdString(item);
            else
                return QVariant::fromValue(item);
        },
        value);
}

QtFeatureManager::ChangeKind toQtChangeKind(FeatureChangeKind kind)
{
    switch (kind)
    {
    case FeatureChangeKind::Added:
        return QtFeatureManager::ChangeKind::Added;
    case FeatureChangeKind::Updated:
        return QtFeatureManager::ChangeKind::Updated;
    case FeatureChangeKind::Removed:
        return QtFeatureManager::ChangeKind::Removed;
    }
    return QtFeatureManager::ChangeKind::Updated;
}

QStringList toQStringList(const std::vector<std::string> &values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const auto &value : values)
        result.append(QString::fromStdString(value));
    return result;
}

void logProviderWarnings(const QStringList &warnings)
{
    for (const auto &warning : warnings)
        qWarning() << warning;
}
} // namespace

struct QtFeatureManager::Impl
{
    FeatureManager core;
    FeatureSubscription subscription;
    QFileSystemWatcher *watcher = nullptr;
    QTimer *reloadTimer         = nullptr;
    QString filePath;
    QString directoryPath;
};

QtFeatureManager &QtFeatureManager::instance()
{
    auto *application = QCoreApplication::instance();
    if (!application)
        qFatal("QtFeatureManager requires a QCoreApplication");

    if (QThread::currentThread() != application->thread())
    {
        QtFeatureManager *manager = nullptr;
        const bool invoked        = QMetaObject::invokeMethod(
            application, [&manager] { manager = &QtFeatureManager::instance(); }, Qt::BlockingQueuedConnection);
        if (!invoked || !manager)
            qFatal("Unable to create QtFeatureManager on the application thread");
        return *manager;
    }

    static QPointer<QtFeatureManager> manager;
    if (!manager)
        manager = new QtFeatureManager(application);
    return *manager;
}

QtFeatureManager::QtFeatureManager(QObject *parent) :
    QObject(parent),
    m_impl(std::make_unique<Impl>())
{
    m_impl->watcher     = new QFileSystemWatcher(this);
    m_impl->reloadTimer = new QTimer(this);
    m_impl->reloadTimer->setSingleShot(true);
    m_impl->reloadTimer->setInterval(kReloadDebounce);
    connect(m_impl->watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) { scheduleReload(); });
    connect(m_impl->watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &)
            {
                if (QFileInfo::exists(m_impl->filePath))
                    scheduleReload();
            });
    connect(m_impl->reloadTimer, &QTimer::timeout, this, &QtFeatureManager::reloadFromProvider);

    m_impl->subscription = m_impl->core.subscribe(
        [this](const FeatureChange &change)
        {
            const QString name     = QString::fromStdString(change.name);
            const QVariant value   = change.kind == FeatureChangeKind::Removed ? QVariant{} : toQVariant(change.value);
            const auto kind        = toQtChangeKind(change.kind);
            const quint64 revision = change.revision;
            QMetaObject::invokeMethod(
                this, [this, name, kind, value, revision] { emit featureChanged(name, kind, value, revision); },
                Qt::QueuedConnection);
        });
}

QtFeatureManager::~QtFeatureManager() = default;

QtFeatureOperationResult QtFeatureManager::useLocalFile(const QString &filePath)
{
    if (QThread::currentThread() != thread())
        return {false, QStringLiteral("Feature configuration must be installed on the application thread")};

    const QFileInfo fileInfo(filePath);
    const QString absolutePath  = fileInfo.absoluteFilePath();
    const QString directoryPath = fileInfo.absoluteDir().absolutePath();
#ifdef _WIN32
    auto path = std::filesystem::path(absolutePath.toStdWString());
#else
    auto path = std::filesystem::path(absolutePath.toUtf8().toStdString());
#endif
    auto result = setProvider(makeLocalFileFeatureProvider(std::move(path)));
    if (!result.ok)
        return result;

    m_impl->reloadTimer->stop();
    if (!m_impl->filePath.isEmpty())
        m_impl->watcher->removePath(m_impl->filePath);
    if (!m_impl->directoryPath.isEmpty())
        m_impl->watcher->removePath(m_impl->directoryPath);
    m_impl->filePath      = absolutePath;
    m_impl->directoryPath = directoryPath;

    const bool directoryWatched =
        m_impl->watcher->directories().contains(directoryPath) || m_impl->watcher->addPath(directoryPath);
    const bool fileWatched = m_impl->watcher->files().contains(absolutePath) || m_impl->watcher->addPath(absolutePath);
    if (!directoryWatched || !fileWatched)
    {
        const QString message = QStringLiteral("Unable to watch feature configuration: %1").arg(absolutePath);
        emit reloadFailed(message);
        return {false, message};
    }
    return result;
}

QtFeatureOperationResult QtFeatureManager::setProvider(std::unique_ptr<IFeatureProvider> provider)
{
    const auto result = m_impl->core.setProvider(std::move(provider));
    if (!result.ok)
    {
        const QString message = QString::fromStdString(result.message);
        emit reloadFailed(message);
        return {false, message};
    }
    const auto warnings = toQStringList(result.value.providerStatus.warnings);
    logProviderWarnings(warnings);
    emit providerStatusChanged(result.value.providerStatus.stale, warnings);
    const quint64 revision = result.value.revision;
    QMetaObject::invokeMethod(this, [this, revision] { emit featuresReloaded(revision); }, Qt::QueuedConnection);
    return {true, {}};
}

void QtFeatureManager::scheduleReload()
{
    m_impl->reloadTimer->start();
}

void QtFeatureManager::reloadFromProvider()
{
    if (!QFileInfo::exists(m_impl->filePath))
    {
        emit reloadFailed(QStringLiteral("Feature configuration is temporarily unavailable: %1").arg(m_impl->filePath));
        return;
    }
    if (!m_impl->watcher->files().contains(m_impl->filePath) && !m_impl->watcher->addPath(m_impl->filePath))
    {
        emit reloadFailed(QStringLiteral("Unable to restore feature configuration watch: %1").arg(m_impl->filePath));
        return;
    }

    const auto result = m_impl->core.reload();
    if (!result.ok)
    {
        emit reloadFailed(QString::fromStdString(result.message));
        return;
    }

    const auto warnings = toQStringList(result.value.providerStatus.warnings);
    logProviderWarnings(warnings);
    emit providerStatusChanged(result.value.providerStatus.stale, warnings);
    const quint64 revision = result.value.revision;
    QMetaObject::invokeMethod(this, [this, revision] { emit featuresReloaded(revision); }, Qt::QueuedConnection);
}

bool QtFeatureManager::isEnabled(const QString &name, bool defaultValue) const
{
    return m_impl->core.isEnabled(name.toStdString(), defaultValue);
}

QVariant QtFeatureManager::value(const QString &name, const QVariant &defaultValue) const
{
    const auto found = m_impl->core.findValue(name.toStdString());
    return found ? toQVariant(*found) : defaultValue;
}
