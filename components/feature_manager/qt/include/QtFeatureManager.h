/**
 * @file QtFeatureManager.h
 * @brief 将 FeatureManager 暴露给 Qt/QML 的单例适配器。
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <memory>

namespace SZX::FEATURE
{
class IFeatureProvider;
}

/** @brief QML 友好的 Feature 操作结果。 */
struct QtFeatureOperationResult
{
    bool ok = false;
    QString message;
};

/** @brief 将 Feature 查询、重载和状态变更转发为 Qt 属性与信号。 */
class QtFeatureManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QtFeatureManager)

public:
    enum class ChangeKind
    {
        Added,
        Updated,
        Removed,
    };
    Q_ENUM(ChangeKind)

    /** @brief 返回进程内唯一的 Qt Feature 管理器。 */
    static QtFeatureManager &instance();
    ~QtFeatureManager() override;

    /// 文件扩展名为 .yaml/.yml 时使用 YAML，其余使用 JSON。
    [[nodiscard]] QtFeatureOperationResult useLocalFile(const QString &filePath);

    /** @brief 查询 QML 可用的布尔 Feature。 */
    Q_INVOKABLE [[nodiscard]] bool isEnabled(const QString &featureName, bool defaultValue = false) const;
    Q_INVOKABLE [[nodiscard]] QVariant value(const QString &featureName, const QVariant &defaultValue = {}) const;

signals:
    void featureChanged(const QString &featureName, ChangeKind kind, const QVariant &value, quint64 revision);
    void featuresReloaded(quint64 revision);
    void reloadFailed(const QString &message);
    void providerStatusChanged(bool stale, const QStringList &warnings);

private:
    explicit QtFeatureManager(QObject *parent = nullptr);
    [[nodiscard]] QtFeatureOperationResult setProvider(std::unique_ptr<SZX::FEATURE::IFeatureProvider> provider);
    void scheduleReload();
    void reloadFromProvider();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
