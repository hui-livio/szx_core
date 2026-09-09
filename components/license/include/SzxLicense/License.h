#pragma once

/**
 * @file License.h
 * @brief 许可证请求、签发和验证的公共数据结构及 API。
 *
 * 请求文件使用签发方公钥加密，许可证文件使用签发方私钥签名。
 * 本头文件只暴露值类型和 Result 返回值，不暴露 OpenSSL 或其他第三方类型。
 */

#include "Result.hpp"
#include "SzxLicense/LicenseExport.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::LICENSE
{
/**
 * @brief 许可证操作的稳定错误码。
 *
 * 错误码会通过 Result 跨越模块边界，调用方不应依赖错误消息文本。
 */
enum class LicenseError : int
{
    InvalidArgument  = 1800, ///< 输入参数为空、超出限制或不满足约束。
    Malformed        = 1801, ///< 请求或许可证数据格式错误。
    Unsupported      = 1802, ///< 数据版本或算法不受支持。
    SignatureInvalid = 1803, ///< 许可证签名校验失败。
    Expired          = 1804, ///< 许可证已过期。
    NotYetValid      = 1805, ///< 许可证尚未到生效时间。
    WrongProduct     = 1806, ///< 许可证产品与预期产品不一致。
    MachineMismatch  = 1807, ///< 当前机器因子与许可证不匹配。
    IoError          = 1808, ///< 文件读取、写入或提交失败。
};

/**
 * @brief 经过哈希处理的机器绑定因子。
 *
 * 原始机器标识不会通过该类型返回；hash 为十六进制编码的摘要值。
 */
struct MachineFactor
{
    std::string type; ///< 因子类型，例如 machine_id 或 smbios_uuid。
    std::string hash; ///< 因子摘要，使用小写十六进制字符串表示。
};

/**
 * @brief 创建许可证请求文件时使用的参数。
 */
struct RequestOptions
{
    std::string productId;                           ///< 产品标识，不能为空。
    std::string appVersion;                          ///< 应用版本，不能为空。
    std::vector<std::string> requestedFeatures;      ///< 请求的功能标识列表。
    std::map<std::string, std::string> customFields; ///< 请求附带的自定义字段。
};

/**
 * @brief 签发许可证时使用的参数。
 */
struct IssueOptions
{
    std::string licenseId;                           ///< 许可证标识，不能为空。
    std::string issuer;                              ///< 签发方标识，不能为空。
    std::vector<std::string> features;               ///< 许可证功能列表；为空时继承请求值。
    std::map<std::string, std::string> customFields; ///< 许可证自定义字段。
    std::int64_t notBeforeUtc{};                     ///< UTC Unix 时间戳；为零时使用当前签发时间。
    std::optional<std::int64_t> expiresAtUtc;        ///< 可选的 UTC 到期时间；缺省表示永久。
};

/**
 * @brief 验证成功后解析出的许可证信息。
 */
struct LicenseInfo
{
    std::string licenseId;                           ///< 许可证标识。
    std::string productId;                           ///< 产品标识。
    std::string appVersion;                          ///< 应用版本。
    std::string issuer;                              ///< 签发方标识。
    std::int64_t issuedAtUtc{};                      ///< 签发时间，UTC Unix 时间戳。
    std::int64_t notBeforeUtc{};                     ///< 生效时间，UTC Unix 时间戳。
    std::optional<std::int64_t> expiresAtUtc;        ///< 到期时间；缺省表示永久。
    std::vector<std::string> features;               ///< 许可证功能列表。
    std::vector<MachineFactor> machineFactors;       ///< 许可证绑定的机器因子。
    std::map<std::string, std::string> customFields; ///< 许可证自定义字段。
};

/**
 * @brief 收集当前机器的产品级机器因子摘要。
 *
 * @param[in] productId 产品标识，用于隔离不同产品的摘要空间。
 * @return 成功时返回机器因子列表；失败时返回稳定错误码。
 * @note 原始硬件标识不会从该 API 返回。
 */
[[nodiscard]] SZX_LICENSE_API auto collectMachineFactors(std::string_view productId)
    -> SZX::COMMON::Result<std::vector<MachineFactor>>;

/**
 * @brief 创建未加密的 UTF-8 JSON 请求内容。
 *
 * @param[in] options 请求参数。
 * @return 成功时返回请求 JSON；失败时返回稳定错误码。
 * @pre productId 和 appVersion 不能为空。
 */
[[nodiscard]] SZX_LICENSE_API auto createRequest(const RequestOptions &options) -> SZX::COMMON::Result<std::string>;

/**
 * @brief 创建并原子写入加密的许可证请求文件。
 *
 * @param[in] options 请求参数。
 * @param[in] outputPath 输出文件路径；目标文件已存在时操作失败。
 * @param[in] issuerPublicKey 签发方 RSA 公钥，支持基础库定义的密钥字节格式。
 * @return 成功时返回空结果；失败时返回稳定错误码。
 * @post 成功时输出文件为带认证保护的请求封装，通常使用 .szxreq 扩展名。
 */
[[nodiscard]] SZX_LICENSE_API auto createRequestFile(const RequestOptions &options, std::string_view outputPath,
                                                     std::span<const std::uint8_t> issuerPublicKey) -> SZX::COMMON::Result<void>;

/**
 * @brief 在内存中解析并验证许可证封装。
 *
 * @param[in] envelope 许可证二进制封装。
 * @param[in] issuerPublicKey 用于验证签名的签发方 RSA 公钥。
 * @param[in] expectedProduct 可选的预期产品标识，空值表示不限定产品。
 * @return 成功时返回解析后的许可证信息；失败时返回稳定错误码。
 */
[[nodiscard]] SZX_LICENSE_API auto verify(std::span<const std::uint8_t> envelope, std::span<const std::uint8_t> issuerPublicKey,
                                          std::string_view expectedProduct = {}) -> SZX::COMMON::Result<LicenseInfo>;

/**
 * @brief 从文件读取、解密并验证许可证封装。
 *
 * @param[in] licensePath 许可证文件路径。
 * @param[in] issuerPublicKey 用于验证签名的签发方 RSA 公钥。
 * @param[in] expectedProduct 可选的预期产品标识，空值表示不限定产品。
 * @return 成功时返回解析后的许可证信息；失败时返回稳定错误码。
 * @note 输入文件大小受基础库上限约束。
 */
[[nodiscard]] SZX_LICENSE_API auto verifyFile(std::string_view licensePath, std::span<const std::uint8_t> issuerPublicKey,
                                              std::string_view expectedProduct = {}) -> SZX::COMMON::Result<LicenseInfo>;

/**
 * @brief 判断许可证是否包含指定功能。
 *
 * @param[in] license 待查询的许可证信息。
 * @param[in] feature 功能标识。
 * @return 包含该功能时返回 true，否则返回 false。
 */
[[nodiscard]] SZX_LICENSE_API auto hasFeature(const LicenseInfo &license, std::string_view feature) noexcept -> bool;
} // namespace SZX::LICENSE
