#include "IssuerGuiController.h"

#include "FileHelper.hpp"
#include "KeyFileHelper.h"
#include "Serializer.hpp"
#include "SzxCrypto/Rsa.h"
#include "SzxLicense/Issuer.h"
#include "SzxLicense/License.h"


#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>


struct KeyRecord
{
    std::string id, projectId, cryptoKeyId, privatePath, publicPath;
    std::int64_t createdAt = 0;
};
SZX_REFL(KeyRecord, id, projectId, cryptoKeyId, privatePath, publicPath, createdAt)

struct LegacyKeyRecord
{
    std::string id, projectId, cryptoKeyId, privatePath, publicPath, createdAt;
};
SZX_REFL(LegacyKeyRecord, id, projectId, cryptoKeyId, privatePath, publicPath, createdAt)

namespace
{
auto validPathPart(const QString &value) -> bool
{
    if (value.isEmpty() || value == QStringLiteral(".") || value == QStringLiteral(".."))
        return false;
    for (const auto ch : value)
        if (!(ch.isLetterOrNumber() || ch == u'_' || ch == u'-' || ch == u'.'))
            return false;
    return true;
}
auto keyRoot() -> QString
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base.isEmpty() ? QString{} : QDir(base).filePath(QStringLiteral("datasets"));
}
auto clear(std::vector<std::uint8_t> &bytes) noexcept
{
    std::fill(bytes.begin(), bytes.end(), 0);
    bytes.clear();
}
struct SensitiveBytesGuard
{
    std::vector<std::uint8_t> *bytes = nullptr;
    ~SensitiveBytesGuard()
    {
        if (bytes)
            clear(*bytes);
    }
};
auto writeKeyPairFiles(const QString &privatePath, const QString &publicPath, const std::vector<std::uint8_t> &privatePem,
                       const std::vector<std::uint8_t> &publicPem, bool overwrite) -> bool
{
    const auto privateTemp   = privatePath + QStringLiteral(".new");
    const auto publicTemp    = publicPath + QStringLiteral(".new");
    const auto privateBackup = privatePath + QStringLiteral(".bak");
    const auto publicBackup  = publicPath + QStringLiteral(".bak");
    QFile::remove(privateTemp);
    QFile::remove(publicTemp);
    QFile::remove(privateBackup);
    QFile::remove(publicBackup);
    if (!overwrite && (QFile::exists(privatePath) || QFile::exists(publicPath)))
        return false;
    if (!SzxLicenseIssuerGui::writeBytes(privateTemp.toStdString(), privatePem, true) ||
        !SzxLicenseIssuerGui::writeBytes(publicTemp.toStdString(), publicPem, true))
    {
        QFile::remove(privateTemp);
        QFile::remove(publicTemp);
        return false;
    }
    const bool hadPrivate = QFile::exists(privatePath), hadPublic = QFile::exists(publicPath);
    if ((hadPrivate && !QFile::rename(privatePath, privateBackup)) || (hadPublic && !QFile::rename(publicPath, publicBackup)))
    {
        if (hadPrivate && QFile::exists(privateBackup))
            QFile::rename(privateBackup, privatePath);
        if (hadPublic && QFile::exists(publicBackup))
            QFile::rename(publicBackup, publicPath);
        QFile::remove(privateTemp);
        QFile::remove(publicTemp);
        return false;
    }
    if (!QFile::rename(privateTemp, privatePath) || !QFile::rename(publicTemp, publicPath))
    {
        QFile::remove(privatePath);
        QFile::remove(publicPath);
        if (hadPrivate)
            QFile::rename(privateBackup, privatePath);
        if (hadPublic)
            QFile::rename(publicBackup, publicPath);
        QFile::remove(privateTemp);
        QFile::remove(publicTemp);
        return false;
    }
    QFile::remove(privateBackup);
    QFile::remove(publicBackup);
    return true;
}
auto deleteKeyPairFiles(const QString &privatePath, const QString &publicPath) -> bool
{
    const auto privateBackup = privatePath + QStringLiteral(".delete");
    const auto publicBackup  = publicPath + QStringLiteral(".delete");
    QFile::remove(privateBackup);
    QFile::remove(publicBackup);
    const bool hadPrivate = QFile::exists(privatePath), hadPublic = QFile::exists(publicPath);
    if ((hadPrivate && !QFile::rename(privatePath, privateBackup)) || (hadPublic && !QFile::rename(publicPath, publicBackup)))
    {
        if (hadPrivate && QFile::exists(privateBackup))
            QFile::rename(privateBackup, privatePath);
        if (hadPublic && QFile::exists(publicBackup))
            QFile::rename(publicBackup, publicPath);
        return false;
    }
    if ((hadPrivate && !QFile::remove(privateBackup)) || (hadPublic && !QFile::remove(publicBackup)))
    {
        if (hadPrivate && QFile::exists(privateBackup))
            QFile::rename(privateBackup, privatePath);
        if (hadPublic && QFile::exists(publicBackup))
            QFile::rename(publicBackup, publicPath);
        return false;
    }
    return true;
}
} // namespace

IssuerGuiController::IssuerGuiController(QObject *parent) :
    QObject(parent)
{
    loadKeyRecords();
}

void IssuerGuiController::loadKeyRecords()
{
    constexpr qint64 maxRecordBytes = 2 * 1024 * 1024;
    const auto root                 = keyRoot();
    if (root.isEmpty())
        return;
    const auto path = root + QStringLiteral("/key-records.json");
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.size() > maxRecordBytes)
        return;
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return;
    const auto bytes = input.readAll();
    if (input.error() != QFileDevice::NoError || bytes.isEmpty())
        return;
    const std::string text(bytes.constData(), static_cast<size_t>(bytes.size()));
    std::vector<KeyRecord> records;
    bool migratedLegacyTimestamps = false;
    try
    {
        SZX::COMMON::Serializer::from_json(records, text);
    } catch (...)
    {
        // Records written by older builds used an ISO-8601 string. Convert those
        // values once on load; all subsequent saves use numeric UTC timestamps.
        std::vector<LegacyKeyRecord> legacy;
        try
        {
            SZX::COMMON::Serializer::from_json(legacy, text);
            migratedLegacyTimestamps = true;
            records.reserve(legacy.size());
            for (const auto &old : legacy)
            {
                const auto parsed = QDateTime::fromString(QString::fromStdString(old.createdAt), Qt::ISODate);
                records.push_back({old.id, old.projectId, old.cryptoKeyId, old.privatePath, old.publicPath,
                                   parsed.isValid() ? parsed.toSecsSinceEpoch() : 0});
            }
        } catch (...)
        {
            return;
        }
    }
    for (const auto &r : records)
    {
        const auto id                = QString::fromStdString(r.id);
        const auto projectId         = QString::fromStdString(r.projectId);
        const auto expectedDirectory = QDir::cleanPath(keyRoot() + QStringLiteral("/") + projectId);
        const auto expectedPrivate   = expectedDirectory + QStringLiteral("/") + id + QStringLiteral(".pri");
        const auto expectedPublic    = expectedDirectory + QStringLiteral("/") + id + QStringLiteral(".pub");
        if (!validPathPart(id) || !validPathPart(projectId) || r.privatePath.empty() || r.publicPath.empty() ||
            QDir::cleanPath(QString::fromStdString(r.privatePath)) != expectedPrivate ||
            QDir::cleanPath(QString::fromStdString(r.publicPath)) != expectedPublic)
            continue;
        bool duplicate = false;
        for (const auto &existing : keyRecords_)
            if (existing.toMap().value(QStringLiteral("id")).toString() == id &&
                existing.toMap().value(QStringLiteral("projectId")).toString() == projectId)
                duplicate = true;
        if (duplicate)
            continue;
        QVariantMap m;
        m.insert("id", id);
        m.insert("projectId", projectId);
        m.insert("cryptoKeyId", QString::fromStdString(r.cryptoKeyId));
        m.insert("privatePath", QString::fromStdString(r.privatePath));
        m.insert("publicPath", QString::fromStdString(r.publicPath));
        m.insert("createdAt", static_cast<qint64>(r.createdAt));
        keyRecords_.append(m);
    }
    if (migratedLegacyTimestamps)
    {
        try
        {
            (void)saveKeyRecords();
        } catch (...)
        {
            // Keep the in-memory records usable even if migration cannot be persisted.
        }
    }
}

bool IssuerGuiController::saveKeyRecords() const
{
    const auto dir = keyRoot();
    if (dir.isEmpty() || !QDir().mkpath(dir))
        return false;
    std::vector<KeyRecord> records;
    for (const auto &item : keyRecords_)
    {
        const auto m = item.toMap();
        records.push_back({m.value("id").toString().toStdString(), m.value("projectId").toString().toStdString(),
                           m.value("cryptoKeyId").toString().toStdString(), m.value("privatePath").toString().toStdString(),
                           m.value("publicPath").toString().toStdString(), m.value("createdAt").toLongLong()});
    }
    std::string text;
    SZX::COMMON::Serializer::to_json(records, text);
    QSaveFile output(dir + QStringLiteral("/key-records.json"));
    if (!output.open(QIODevice::WriteOnly) || output.write(QByteArray::fromStdString(text)) != static_cast<qint64>(text.size()))
        return false;
    return output.commit();
}

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
    busy_          = false;
    lastErrorCode_ = errorCode;
    status_        = success ? QStringLiteral("success") : QStringLiteral("error");
    statusMessage_ = message;
    emit busyChanged();
    emit statusChanged();
    emit operationFinished(success, errorCode, message);
}

bool IssuerGuiController::inspectPublicKey(const QString &path)
{
    if (!beginOperation())
        return false;
    try
    {
        auto key = SzxLicenseIssuerGui::readPublicKey(path.toStdString());
        if (!key)
        {
            finish(false, 1710, QStringLiteral("无法读取或解析公钥。"));
            return false;
        }
        auto id = SZX::CRYPTO::rsaPublicKeyId(*key);
        if (!id.ok)
        {
            finish(false, id.error, QString::fromStdString(id.message));
            return false;
        }
        issuerKeyId_    = QString::fromStdString(id.value);
        keyPairMatched_ = false;
        emit keyInfoChanged();
        finish(true, 0, QStringLiteral("公钥检查成功。"));
        return true;
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("公钥检查失败。"));
        return false;
    }
}

bool IssuerGuiController::inspectKeyPair(const QString &privatePath, const QString &publicPath)
{
    if (!beginOperation())
        return false;
    try
    {
        auto privateKey = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
        auto publicKey  = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!privateKey || !publicKey)
        {
            finish(false, 1710, QStringLiteral("无法读取密钥文件。"));
            return false;
        }
        SensitiveBytesGuard privateGuard{&*privateKey};
        auto id    = SZX::CRYPTO::rsaPublicKeyId(*publicKey);
        auto probe = SZX::CRYPTO::rsaPssSha256Sign(*privateKey, std::span<const std::uint8_t>{});
        const std::span<const std::uint8_t> emptyData;
        auto verification = probe.ok ? SZX::CRYPTO::rsaPssSha256Verify(*publicKey, emptyData, probe.value) :
                                       SZX::COMMON::Result<bool>::Failure(1710, "key probe failed");
        auto matched      = id.ok && probe.ok && verification.ok && verification.value;
        issuerKeyId_      = id.ok ? QString::fromStdString(id.value) : QString{};
        keyPairMatched_   = matched;
        emit keyInfoChanged();
        finish(matched, matched ? 0 : 1710, matched ? QStringLiteral("公私钥匹配。") : QStringLiteral("公私钥不匹配。"));
        return matched;
    } catch (...)
    {
        finish(false, 1710, QStringLiteral("密钥检查失败。"));
        return false;
    }
}

bool IssuerGuiController::generateKeyPair(const QString &privatePath, const QString &publicPath, bool overwrite)
{
    if (!beginOperation())
        return false;
    try
    {
        const auto privatePathString = privatePath.toStdString();
        const auto publicPathString  = publicPath.toStdString();
        if (privatePathString.empty() || publicPathString.empty() || privatePathString == publicPathString)
        {
            finish(false, 1710, QStringLiteral("请填写不同的私钥和公钥路径。"));
            return false;
        }
        const auto privateTarget = std::filesystem::absolute(std::filesystem::path(privatePathString)).lexically_normal();
        const auto publicTarget  = std::filesystem::absolute(std::filesystem::path(publicPathString)).lexically_normal();
        if (privateTarget == publicTarget)
        {
            finish(false, 1710, QStringLiteral("请填写不同的私钥和公钥路径。"));
            return false;
        }
        if (!overwrite && (std::filesystem::exists(privateTarget) || std::filesystem::exists(publicTarget)))
        {
            finish(false, 1710, QStringLiteral("密钥文件已存在，请勾选覆盖后重试。"));
            return false;
        }

        auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
        if (!keys.ok)
        {
            finish(false, 1710, QStringLiteral("密钥生成或写入失败。"));
            return false;
        }
        SensitiveBytesGuard keyGuard{&keys.value.privateKey};

        // Confirm the generated material before publishing it to disk.
        const std::span<const std::uint8_t> emptyData;
        const auto probe    = SZX::CRYPTO::rsaPssSha256Sign(keys.value.privateKey, emptyData);
        const auto verified = probe.ok ? SZX::CRYPTO::rsaPssSha256Verify(keys.value.publicKey, emptyData, probe.value) :
                                         SZX::COMMON::Result<bool>::Failure(1711, "key probe failed");
        if (!probe.ok || !verified.ok || !verified.value)
        {
            clear(keys.value.privateKey);
            finish(false, 1711, QStringLiteral("生成的密钥对校验失败。"));
            return false;
        }

        auto privatePem      = SZX::CRYPTO::rsaPrivateKeyToPem(keys.value.privateKey);
        const auto publicPem = SZX::CRYPTO::rsaPublicKeyToPem(keys.value.publicKey);
        SensitiveBytesGuard privatePemGuard{privatePem.ok ? &privatePem.value : nullptr};
        if (!privatePem.ok || !publicPem.ok)
        {
            clear(keys.value.privateKey);
            finish(false, 1711, QStringLiteral("PEM 密钥编码失败。"));
            return false;
        }
        const auto publicWritten = writeKeyPairFiles(privatePath, publicPath, privatePem.value, publicPem.value, overwrite);
        if (!publicWritten)
        {
            clear(keys.value.privateKey);
            finish(false, 1710, QStringLiteral("密钥生成或写入失败。"));
            return false;
        }
        auto id = SZX::CRYPTO::rsaPublicKeyId(keys.value.publicKey);
        if (!id.ok)
        {
            clear(keys.value.privateKey);
            finish(false, id.error, QStringLiteral("无法计算生成公钥的 Key ID。"));
            return false;
        }
        issuerKeyId_    = QString::fromStdString(id.value);
        keyPairMatched_ = true;
        clear(keys.value.privateKey);
        emit keyInfoChanged();
        finish(true, 0, QStringLiteral("RSA-3072 密钥对生成成功，已写入并校验。"));
        return true;
    } catch (...)
    {
        finish(false, 1711, QStringLiteral("密钥生成失败。"));
        return false;
    }
}

bool IssuerGuiController::importKeyPair(const QString &privatePath, const QString &publicPath, const QString &projectId,
                                        const QString &keyPairId, bool overwrite)
{
    if (!beginOperation())
        return false;
    try
    {
        auto privateKey      = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
        const auto publicKey = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!privateKey || !publicKey || projectId.trimmed().isEmpty())
        {
            finish(false, 1710, QStringLiteral("请提供有效的密钥文件和项目 ID。"));
            return false;
        }
        if (!validPathPart(projectId.trimmed()) || !validPathPart(keyPairId.trimmed()))
        {
            finish(false, 1710, QStringLiteral("项目 ID 和密钥对 ID 只能包含字母、数字、点、下划线或连字符。"));
            return false;
        }
        const auto cryptoId = SZX::CRYPTO::rsaPublicKeyId(*publicKey);
        const auto probe    = SZX::CRYPTO::rsaPssSha256Sign(*privateKey, std::span<const std::uint8_t>{});
        const std::span<const std::uint8_t> emptyData;
        const auto verified = probe.ok ? SZX::CRYPTO::rsaPssSha256Verify(*publicKey, emptyData, probe.value) :
                                         SZX::COMMON::Result<bool>::Failure(1710, "probe failed");
        if (!cryptoId.ok || !probe.ok || !verified.ok || !verified.value)
        {
            finish(false, 1710, QStringLiteral("公私钥无效或不匹配。"));
            return false;
        }
        const auto project = projectId.trimmed();
        const auto pairId  = keyPairId.trimmed();
        for (const auto &existing : keyRecords_)
            if (existing.toMap().value(QStringLiteral("id")) == pairId &&
                existing.toMap().value(QStringLiteral("projectId")) == project)
            {
                finish(false, 1712, QStringLiteral("该项目下的密钥对 ID 已存在。"));
                return false;
            }
        const auto destination = keyRoot() + QStringLiteral("/") + project;
        if (!QDir().mkpath(destination))
        {
            finish(false, 1808, QStringLiteral("无法创建密钥工作目录。"));
            return false;
        }
        SensitiveBytesGuard privateGuard{&*privateKey};
        const auto storedPrivate = destination + QStringLiteral("/") + pairId + QStringLiteral(".pri");
        const auto storedPublic  = destination + QStringLiteral("/") + pairId + QStringLiteral(".pub");
        auto privatePem          = SZX::CRYPTO::rsaPrivateKeyToPem(*privateKey);
        const auto publicPem     = SZX::CRYPTO::rsaPublicKeyToPem(*publicKey);
        SensitiveBytesGuard privatePemGuard{privatePem.ok ? &privatePem.value : nullptr};
        if (!privatePem.ok || !publicPem.ok)
        {
            finish(false, 1711, QStringLiteral("PEM 密钥编码失败。"));
            return false;
        }
        if (!writeKeyPairFiles(storedPrivate, storedPublic, privatePem.value, publicPem.value, overwrite))
        {
            finish(false, 1710, QStringLiteral("密钥文件无法保存到工作目录。"));
            return false;
        }
        QVariantMap record;
        record.insert(QStringLiteral("id"), pairId);
        record.insert(QStringLiteral("projectId"), project);
        record.insert(QStringLiteral("cryptoKeyId"), QString::fromStdString(cryptoId.value));
        record.insert(QStringLiteral("privatePath"), storedPrivate);
        record.insert(QStringLiteral("publicPath"), storedPublic);
        record.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
        keyRecords_.append(record);
        if (!saveKeyRecords())
        {
            keyRecords_.removeLast();
            QFile::remove(storedPrivate);
            QFile::remove(storedPublic);
            finish(false, 1808, QStringLiteral("密钥记录保存失败。"));
            return false;
        }
        emit keyRecordsChanged();
        finish(true, 0, QStringLiteral("密钥对已导入。"));
        return true;
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("导入密钥对失败。"));
        return false;
    }
}

bool IssuerGuiController::generateProjectKeyPair(const QString &projectId, const QString &keyPairId, bool overwrite)
{
    const auto project = projectId.trimmed();
    const auto pairId  = keyPairId.trimmed();
    if (!validPathPart(project) || !validPathPart(pairId))
    {
        finish(false, 1710, QStringLiteral("项目 ID 和密钥对 ID 不能为空，且只能包含字母、数字、点、下划线或连字符。"));
        return false;
    }
    const auto destination = keyRoot() + QStringLiteral("/") + project;
    if (keyRoot().isEmpty() || !QDir().mkpath(destination))
    {
        finish(false, 1808, QStringLiteral("无法创建密钥工作目录。"));
        return false;
    }
    const auto privatePath = destination + QStringLiteral("/") + pairId + QStringLiteral(".pri");
    const auto publicPath  = destination + QStringLiteral("/") + pairId + QStringLiteral(".pub");
    bool recordExists      = false;
    for (const auto &record : keyRecords_)
        if (record.toMap().value(QStringLiteral("id")).toString() == pairId &&
            record.toMap().value(QStringLiteral("projectId")).toString() == project)
            recordExists = true;
    if (recordExists && !overwrite)
    {
        finish(false, 1712, QStringLiteral("该项目下的密钥对 ID 已存在，请勾选覆盖后重试。"));
        return false;
    }
    if (!generateKeyPair(privatePath, publicPath, overwrite))
        return false;
    if (recordExists)
        return updateKeyRecord(project, pairId, project, pairId, privatePath, publicPath);
    return importKeyPair(privatePath, publicPath, project, pairId, true);
}

bool IssuerGuiController::updateKeyRecord(const QString &originalProjectId, const QString &originalKeyPairId,
                                          const QString &projectId, const QString &keyPairId, const QString &privatePath,
                                          const QString &publicPath)
{
    if (!beginOperation())
        return false;
    try
    {
        for (auto &item : keyRecords_)
        {
            const auto oldRecord = item.toMap();
            auto record          = oldRecord;
            if (record.value(QStringLiteral("id")).toString() != originalKeyPairId ||
                record.value(QStringLiteral("projectId")).toString() != originalProjectId)
                continue;
            const auto project = projectId.trimmed();
            const auto pairId  = keyPairId.trimmed();
            if (!validPathPart(project) || !validPathPart(pairId))
            {
                finish(false, 1710, QStringLiteral("项目 ID 和密钥对 ID 无效。"));
                return false;
            }
            for (const auto &other : keyRecords_)
                if (other.toMap().value(QStringLiteral("id")) == pairId &&
                    other.toMap().value(QStringLiteral("projectId")) == project &&
                    (other.toMap().value(QStringLiteral("id")).toString() != originalKeyPairId ||
                     other.toMap().value(QStringLiteral("projectId")).toString() != originalProjectId))
                {
                    finish(false, 1712, QStringLiteral("该项目下的密钥对 ID 已存在。"));
                    return false;
                }
            const auto destination = keyRoot() + QStringLiteral("/") + project;
            if (keyRoot().isEmpty() || !QDir().mkpath(destination))
            {
                finish(false, 1808, QStringLiteral("无法创建密钥工作目录。"));
                return false;
            }
            const auto oldPrivate = record.value(QStringLiteral("privatePath")).toString();
            const auto oldPublic  = record.value(QStringLiteral("publicPath")).toString();
            const auto newPrivate = destination + QStringLiteral("/") + pairId + QStringLiteral(".pri");
            const auto newPublic  = destination + QStringLiteral("/") + pairId + QStringLiteral(".pub");
            if ((newPrivate != oldPrivate && QFile::exists(newPrivate)) || (newPublic != oldPublic && QFile::exists(newPublic)))
            {
                finish(false, 1712, QStringLiteral("目标密钥文件已存在，请先选择其他 ID。"));
                return false;
            }
            auto previousPrivate =
                (newPrivate == oldPrivate) ? SzxLicenseIssuerGui::readBytes(oldPrivate.toStdString()) : std::nullopt;
            const auto previousPublic =
                (newPublic == oldPublic) ? SzxLicenseIssuerGui::readBytes(oldPublic.toStdString()) : std::nullopt;
            SensitiveBytesGuard previousPrivateGuard{previousPrivate ? &*previousPrivate : nullptr};
            if (newPrivate == oldPrivate && newPublic == oldPublic && (!previousPrivate || !previousPublic))
            {
                finish(false, 1710, QStringLiteral("原密钥文件无法读取。"));
                return false;
            }
            auto privateBytes      = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
            const auto publicBytes = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
            SensitiveBytesGuard privateGuard{privateBytes ? &*privateBytes : nullptr};
            auto privatePem      = privateBytes ?
                     SZX::CRYPTO::rsaPrivateKeyToPem(*privateBytes) :
                     SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1710, "private key read failed");
            const auto publicPem = publicBytes ?
                SZX::CRYPTO::rsaPublicKeyToPem(*publicBytes) :
                SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1710, "public key read failed");
            SensitiveBytesGuard privatePemGuard{privatePem.ok ? &privatePem.value : nullptr};
            const auto cryptoId = publicBytes ? SZX::CRYPTO::rsaPublicKeyId(*publicBytes) :
                                                SZX::COMMON::Result<std::string>::Failure(1710, "public key read failed");
            const auto probe    = privateBytes ?
                   SZX::CRYPTO::rsaPssSha256Sign(*privateBytes, std::span<const std::uint8_t>{}) :
                   SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1710, "private key read failed");
            const std::span<const std::uint8_t> emptyData;
            const auto verified = probe.ok && publicBytes ?
                SZX::CRYPTO::rsaPssSha256Verify(*publicBytes, emptyData, probe.value) :
                SZX::COMMON::Result<bool>::Failure(1710, "key probe failed");
            if (!privatePem.ok || !publicPem.ok || !cryptoId.ok || !verified.ok || !verified.value ||
                !writeKeyPairFiles(newPrivate, newPublic, privatePem.value, publicPem.value, true))
            {
                finish(false, 1710, QStringLiteral("原密钥文件无法读取或转换。"));
                return false;
            }
            record.insert(QStringLiteral("projectId"), project);
            record.insert(QStringLiteral("id"), pairId);
            record.insert(QStringLiteral("cryptoKeyId"), QString::fromStdString(cryptoId.value));
            record.insert(QStringLiteral("privatePath"), newPrivate);
            record.insert(QStringLiteral("publicPath"), newPublic);
            item = record;
            if (!saveKeyRecords())
            {
                item = oldRecord;
                if (oldPrivate == newPrivate && oldPublic == newPublic && previousPrivate && previousPublic)
                    writeKeyPairFiles(oldPrivate, oldPublic, *previousPrivate, *previousPublic, true);
                else
                {
                    if (oldPrivate != newPrivate)
                        QFile::remove(newPrivate);
                    if (oldPublic != newPublic)
                        QFile::remove(newPublic);
                }
                finish(false, 1808, QStringLiteral("密钥记录保存失败。"));
                return false;
            }
            const bool oldPrivateRemoved = oldPrivate == newPrivate || !QFile::exists(oldPrivate) || QFile::remove(oldPrivate);
            const bool oldPublicRemoved  = oldPublic == newPublic || !QFile::exists(oldPublic) || QFile::remove(oldPublic);
            if (!oldPrivateRemoved || !oldPublicRemoved)
            {
                emit keyRecordsChanged();
                finish(false, 1808, QStringLiteral("记录已更新，但旧密钥文件清理失败。"));
                return false;
            }
            emit keyRecordsChanged();
            finish(true, 0, QStringLiteral("密钥记录已更新。"));
            return true;
        }
        finish(false, 1714, QStringLiteral("未找到指定密钥记录。"));
        return false;
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("编辑密钥记录失败。"));
        return false;
    }
}

bool IssuerGuiController::removeKeyRecord(const QString &projectId, const QString &keyPairId)
{
    if (!beginOperation())
        return false;
    try
    {
        for (qsizetype i = 0; i < keyRecords_.size(); ++i)
            if (keyRecords_[i].toMap().value(QStringLiteral("id")).toString() == keyPairId &&
                keyRecords_[i].toMap().value(QStringLiteral("projectId")).toString() == projectId)
            {
                const auto record = keyRecords_[i].toMap();
                keyRecords_.removeAt(i);
                if (!saveKeyRecords())
                {
                    keyRecords_.insert(i, record);
                    finish(false, 1808, QStringLiteral("密钥记录保存失败。"));
                    return false;
                }
                const auto privatePath = record.value(QStringLiteral("privatePath")).toString();
                const auto publicPath  = record.value(QStringLiteral("publicPath")).toString();
                if (!deleteKeyPairFiles(privatePath, publicPath))
                {
                    keyRecords_.insert(i, record);
                    if (!saveKeyRecords())
                    {
                        finish(false, 1808, QStringLiteral("删除失败且记录回滚失败。"));
                        return false;
                    }
                    emit keyRecordsChanged();
                    finish(false, 1808, QStringLiteral("密钥文件删除失败，已保留记录。"));
                    return false;
                }
                emit keyRecordsChanged();
                finish(true, 0, QStringLiteral("密钥记录已删除。"));
                return true;
            }
        finish(false, 1714, QStringLiteral("未找到指定密钥记录。"));
        return false;
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("删除密钥记录失败。"));
        return false;
    }
}

bool IssuerGuiController::issueLicense(const QString &requestPath, const QString &privatePath, const QString &publicPath,
                                       const QVariantMap &options, const QString &outputPath, bool overwrite)
{
    if (!beginOperation())
        return false;
    try
    {
        auto privateKey = SzxLicenseIssuerGui::readPrivateKey(privatePath.toStdString());
        auto publicKey  = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!privateKey || !publicKey)
        {
            finish(false, 1710, QStringLiteral("无法读取密钥文件。"));
            return false;
        }
        SensitiveBytesGuard privateGuard{&*privateKey};
        SZX::LICENSE::IssueOptions issueOptions;
        issueOptions.licenseId    = options.value(QStringLiteral("licenseId")).toString().toStdString();
        issueOptions.issuer       = options.value(QStringLiteral("issuer")).toString().toStdString();
        issueOptions.notBeforeUtc = options.value(QStringLiteral("notBeforeUtc")).toLongLong();
        if (options.value(QStringLiteral("expiresAtUtc")).isValid())
            issueOptions.expiresAtUtc = options.value(QStringLiteral("expiresAtUtc")).toLongLong();
        auto result = SZX::LICENSE::issueFile(requestPath.toStdString(), issueOptions, *privateKey, *publicKey);
        if (!result.ok || !SzxLicenseIssuerGui::writeBytes(outputPath.toStdString(), result.value, overwrite))
        {
            finish(false, result.ok ? 1808 : result.error,
                   result.ok ? QStringLiteral("许可证写入失败。") : QString::fromStdString(result.message));
            return false;
        }
        clear(*privateKey);
        finish(true, 0, QStringLiteral("许可证签发成功。"));
        return true;
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("许可证签发失败。"));
        return false;
    }
}

bool IssuerGuiController::verifyLicense(const QString &licensePath, const QString &publicPath, const QString &expectedProduct)
{
    if (!beginOperation())
        return false;
    try
    {
        auto publicKey = SzxLicenseIssuerGui::readPublicKey(publicPath.toStdString());
        if (!publicKey)
        {
            finish(false, 1710, QStringLiteral("无法读取公钥。"));
            return false;
        }
        auto result = SZX::LICENSE::verifyFile(licensePath.toStdString(), *publicKey, expectedProduct.toStdString());
        if (!result.ok)
        {
            finish(false, result.error, QString::fromStdString(result.message));
            return false;
        }
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
    } catch (...)
    {
        finish(false, 1801, QStringLiteral("许可证验证失败。"));
        return false;
    }
}

void IssuerGuiController::clearSensitiveState()
{
    clear(privateKey_);
    issuerKeyId_.clear();
    keyPairMatched_ = false;
    emit keyInfoChanged();
}
