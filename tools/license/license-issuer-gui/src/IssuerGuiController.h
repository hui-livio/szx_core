#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <vector>

class IssuerGuiController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(int lastErrorCode READ lastErrorCode NOTIFY statusChanged)
    Q_PROPERTY(QString issuerKeyId READ issuerKeyId NOTIFY keyInfoChanged)
    Q_PROPERTY(bool keyPairMatched READ keyPairMatched NOTIFY keyInfoChanged)
    Q_PROPERTY(QVariantList keyRecords READ keyRecords NOTIFY keyRecordsChanged)

public:
    explicit IssuerGuiController(QObject *parent = nullptr);
    Q_DISABLE_COPY_MOVE(IssuerGuiController)

    [[nodiscard]] bool busy() const noexcept
    {
        return busy_;
    }
    [[nodiscard]] QString status() const
    {
        return status_;
    }
    [[nodiscard]] QString statusMessage() const
    {
        return statusMessage_;
    }
    [[nodiscard]] int lastErrorCode() const noexcept
    {
        return lastErrorCode_;
    }
    [[nodiscard]] QString issuerKeyId() const
    {
        return issuerKeyId_;
    }
    [[nodiscard]] bool keyPairMatched() const noexcept
    {
        return keyPairMatched_;
    }
    [[nodiscard]] QVariantList keyRecords() const
    {
        return keyRecords_;
    }

    Q_INVOKABLE bool inspectPublicKey(const QString &path);
    Q_INVOKABLE bool inspectKeyPair(const QString &privatePath, const QString &publicPath);
    Q_INVOKABLE bool generateKeyPair(const QString &privatePath, const QString &publicPath, bool overwrite = false);
    Q_INVOKABLE bool generateProjectKeyPair(const QString &projectId, const QString &keyPairId, bool overwrite = false);
    Q_INVOKABLE bool importKeyPair(const QString &privatePath, const QString &publicPath, const QString &projectId,
                                   const QString &keyPairId, bool overwrite);
    Q_INVOKABLE bool updateKeyRecord(const QString &originalProjectId, const QString &originalKeyPairId, const QString &projectId,
                                     const QString &keyPairId, const QString &privatePath, const QString &publicPath);
    Q_INVOKABLE bool removeKeyRecord(const QString &projectId, const QString &keyPairId);
    Q_INVOKABLE bool issueLicense(const QString &requestPath, const QString &privatePath, const QString &publicPath,
                                  const QVariantMap &options, const QString &outputPath, bool overwrite = false);
    Q_INVOKABLE bool verifyLicense(const QString &licensePath, const QString &publicPath, const QString &expectedProduct = {});
    Q_INVOKABLE void clearSensitiveState();

signals:
    void busyChanged();
    void statusChanged();
    void keyInfoChanged();
    void keyRecordsChanged();
    void operationFinished(bool success, int errorCode, const QString &message);
    void licenseVerified(const QVariantMap &license);

private:
    bool beginOperation();
    void finish(bool success, int errorCode, const QString &message);
    bool busy_      = false;
    QString status_ = QStringLiteral("ready");
    QString statusMessage_;
    int lastErrorCode_ = 0;
    QString issuerKeyId_;
    bool keyPairMatched_ = false;
    std::vector<std::uint8_t> privateKey_;
    QVariantList keyRecords_;
    void loadKeyRecords();
    [[nodiscard]] bool saveKeyRecords() const;
};
