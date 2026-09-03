#pragma once

#include <QObject>
#include <QVariantMap>

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

public:
    explicit IssuerGuiController(QObject *parent = nullptr);

    [[nodiscard]] bool busy() const noexcept { return busy_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QString statusMessage() const { return statusMessage_; }
    [[nodiscard]] int lastErrorCode() const noexcept { return lastErrorCode_; }
    [[nodiscard]] QString issuerKeyId() const { return issuerKeyId_; }
    [[nodiscard]] bool keyPairMatched() const noexcept { return keyPairMatched_; }

    Q_INVOKABLE bool inspectPublicKey(const QString &path);
    Q_INVOKABLE bool inspectKeyPair(const QString &privatePath, const QString &publicPath);
    Q_INVOKABLE bool generateKeyPair(const QString &privatePath, const QString &publicPath, bool overwrite = false);
    Q_INVOKABLE bool issueLicense(const QString &requestPath, const QString &privatePath, const QString &publicPath,
                                  const QVariantMap &options, const QString &outputPath, bool overwrite = false);
    Q_INVOKABLE bool verifyLicense(const QString &licensePath, const QString &publicPath,
                                   const QString &expectedProduct = {});
    Q_INVOKABLE void clearSensitiveState();

signals:
    void busyChanged();
    void statusChanged();
    void keyInfoChanged();
    void operationFinished(bool success, int errorCode, const QString &message);
    void licenseVerified(const QVariantMap &license);

private:
    bool beginOperation();
    void finish(bool success, int errorCode, const QString &message);
    bool busy_ = false;
    QString status_ = QStringLiteral("ready");
    QString statusMessage_;
    int lastErrorCode_ = 0;
    QString issuerKeyId_;
    bool keyPairMatched_ = false;
    std::vector<std::uint8_t> privateKey_;
};
