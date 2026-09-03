#include "IssuerGuiController.h"

#include "Issuer.h"
#include "KeyFileHelper.h"
#include "SzxCrypto/Rsa.h"
#include "SzxLicense/License.h"
#include "FileHelper.hpp"

#include <algorithm>
#include <filesystem>

namespace
{
auto toBytes(const QString &text) -> std::vector<std::uint8_t>
{
    const auto utf8 = text.toUtf8();
    return {utf8.begin(), utf8.end()};
}
auto clear(std::vector<std::uint8_t> &bytes) noexcept
{
    std::fill(bytes.begin(), bytes.end(), 0);
    bytes.clear();
}
} // namespace

IssuerGuiController::IssuerGuiController(QObject *parent) : QObject(parent) {}

bool IssuerGuiController::beginOperation()
{
    if (busy_)
        return false;
    busy_ = true;
    emit busyChanged();
    return true;
}

void IssuerGuiController::finish(bool success, int errorCode, const QString &message)
{
    busy_ = false;
    lastErrorCode_ = errorCode;
    status_ = success ? QStringLiteral("success") : QStringLiteral("error");
    statusMessage_ = message;
    emit busyChanged();
    emit statusChanged();
    emit operationFinished(success, errorCode, message);
}

bool IssuerGuiController::inspectPublicKey(const QString &path)
{
    if (!beginOperation()) return false;
    try
    {
        auto key = SzxLicenseIssuerGui::readPublicKey(path.toStdString());
        if (!key) { finish(false, 1710, QStringLiteral("无法读取或解析公钥。")); return false; }
        auto id = SZX::CRYPTO::rsaPublicKeyId(*key);
        if (!id.ok) { finish(false, id.error, QString::fromStdString(id.message)); return false; }
        issuerKeyId_ = QString::fromStdString(id.value);
        keyPairMatched_ = false;
        emit keyInfoChanged();
        finish(true, 0, QStringLiteral("公钥检查成功。"));
        return true;
    }
    catch (...) { finish(false, 1801, QStringLiteral("公钥检查失败。")); return false; }
}

bool IssuerGuiController::inspectKeyPair(const QString &privatePath, const QString &publicPath)
{
    if (!beginOperation()) return false;
    try
    {
        auto privateKey = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
        auto publicKey = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!privateKey || !publicKey) { finish(false, 1710, QStringLiteral("无法读取密钥文件。")); return false; }
        auto id = SZX::CRYPTO::rsaPublicKeyId(*publicKey);
        auto probe = SZX::CRYPTO::rsaPssSha256Sign(*privateKey, std::span<const std::uint8_t>{});
        const std::span<const std::uint8_t> emptyData;
        auto verification = probe.ok ? SZX::CRYPTO::rsaPssSha256Verify(*publicKey, emptyData, probe.value)
                                     : SZX::COMMON::Result<bool>::Failure(1710, "key probe failed");
        auto matched = id.ok && probe.ok && verification.ok && verification.value;
        issuerKeyId_ = id.ok ? QString::fromStdString(id.value) : QString{};
        keyPairMatched_ = matched;
        emit keyInfoChanged();
        finish(matched, matched ? 0 : 1710, matched ? QStringLiteral("公私钥匹配。") : QStringLiteral("公私钥不匹配。"));
        return matched;
    }
    catch (...) { finish(false, 1710, QStringLiteral("密钥检查失败。")); return false; }
}

bool IssuerGuiController::generateKeyPair(const QString &privatePath, const QString &publicPath, bool overwrite)
{
    if (!beginOperation()) return false;
    try
    {
        auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
        if (!keys.ok || !SzxLicenseIssuerGui::writeBytes(privatePath.toStdString(), keys.value.privateKey, overwrite) ||
            !SzxLicenseIssuerGui::writeBytes(publicPath.toStdString(), keys.value.publicKey, overwrite))
        { finish(false, 1710, QStringLiteral("密钥生成或写入失败。")); return false; }
        auto id = SZX::CRYPTO::rsaPublicKeyId(keys.value.publicKey);
        issuerKeyId_ = id.ok ? QString::fromStdString(id.value) : QString{};
        keyPairMatched_ = true;
        emit keyInfoChanged();
        finish(true, 0, QStringLiteral("RSA-3072 密钥对生成成功。"));
        return true;
    }
    catch (...) { finish(false, 1711, QStringLiteral("密钥生成失败。")); return false; }
}

bool IssuerGuiController::issueLicense(const QString &requestPath, const QString &privatePath, const QString &publicPath,
                                       const QVariantMap &options, const QString &outputPath, bool overwrite)
{
    if (!beginOperation()) return false;
    try
    {
        auto privateKey = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
        auto publicKey = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!privateKey || !publicKey) { finish(false, 1710, QStringLiteral("无法读取密钥文件。")); return false; }
        SZX::LICENSE::IssueOptions issueOptions;
        issueOptions.licenseId = options.value(QStringLiteral("licenseId")).toString().toStdString();
        issueOptions.issuer = options.value(QStringLiteral("issuer")).toString().toStdString();
        issueOptions.notBeforeUtc = options.value(QStringLiteral("notBeforeUtc")).toLongLong();
        if (options.value(QStringLiteral("expiresAtUtc")).isValid()) issueOptions.expiresAtUtc = options.value(QStringLiteral("expiresAtUtc")).toLongLong();
        auto result = SZX::LICENSE::issueFile(requestPath.toStdString(), issueOptions, *privateKey, *publicKey);
        if (!result.ok || !SzxLicenseIssuerGui::writeBytes(outputPath.toStdString(), result.value, overwrite))
        { finish(false, result.ok ? 1808 : result.error, result.ok ? QStringLiteral("许可证写入失败。") : QString::fromStdString(result.message)); return false; }
        clear(*privateKey);
        finish(true, 0, QStringLiteral("许可证签发成功。"));
        return true;
    }
    catch (...) { finish(false, 1801, QStringLiteral("许可证签发失败。")); return false; }
}

bool IssuerGuiController::verifyLicense(const QString &licensePath, const QString &publicPath, const QString &expectedProduct)
{
    if (!beginOperation()) return false;
    try
    {
        auto publicKey = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!publicKey) { finish(false, 1710, QStringLiteral("无法读取公钥。")); return false; }
        auto result = SZX::LICENSE::verifyFile(licensePath.toStdString(), *publicKey, expectedProduct.toStdString());
        if (!result.ok) { finish(false, result.error, QString::fromStdString(result.message)); return false; }
        QVariantMap view;
        view.insert("licenseId", QString::fromStdString(result.value.licenseId));
        view.insert("productId", QString::fromStdString(result.value.productId));
        view.insert("appVersion", QString::fromStdString(result.value.appVersion));
        view.insert("issuer", QString::fromStdString(result.value.issuer));
        view.insert("issuedAtUtc", result.value.issuedAtUtc);
        view.insert("notBeforeUtc", result.value.notBeforeUtc);
        view.insert("expiresAtUtc", result.value.expiresAtUtc ? QVariant::fromValue(*result.value.expiresAtUtc) : QVariant{});
        QStringList features;
        for (const auto &feature : result.value.features)
            features.append(QString::fromStdString(feature));
        view.insert("features", features);
        emit licenseVerified(view);
        finish(true, 0, QStringLiteral("许可证验证成功。"));
        return true;
    }
    catch (...) { finish(false, 1801, QStringLiteral("许可证验证失败。")); return false; }
}

void IssuerGuiController::clearSensitiveState()
{
    clear(privateKey_);
    issuerKeyId_.clear();
    keyPairMatched_ = false;
    emit keyInfoChanged();
}
