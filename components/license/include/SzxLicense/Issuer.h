#pragma once

/**
 * @file Issuer.h
 * @brief 许可证签发端使用的内部 API。
 *
 * 该头文件供受控的签发工具使用，不作为客户 SDK 的一部分安装。
 */

#include "SzxLicense/License.h"

namespace SZX::LICENSE
{
/**
 * @brief 根据请求 JSON 创建签名许可证封装。
 *
 * @param[in] requestJson 已解密且符合请求格式的 JSON 内容。
 * @param[in] options 签发参数。
 * @param[in] issuerPrivateKey 用于签名的签发方 RSA 私钥。
 * @param[in] issuerPublicKey 用于生成 Key ID 并进行签名自校验的公钥。
 * @return 成功时返回许可证二进制封装；失败时返回稳定错误码。
 * @note 公私钥不匹配时操作失败；私钥材料不会被写入输出结果。
 */
[[nodiscard]] SZX_LICENSE_API auto issue(const std::string &requestJson, const IssueOptions &options,
                                         std::span<const std::uint8_t> issuerPrivateKey,
                                         std::span<const std::uint8_t> issuerPublicKey)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief 读取加密请求文件并创建签名许可证封装。
 *
 * @param[in] requestPath .szxreq 请求文件路径。
 * @param[in] options 签发参数。
 * @param[in] issuerPrivateKey 用于解密请求并签名的签发方 RSA 私钥。
 * @param[in] issuerPublicKey 用于生成 Key ID 并进行签名自校验的公钥。
 * @return 成功时返回许可证二进制封装；失败时返回稳定错误码。
 * @note 该函数只生成内存结果，不负责写入许可证输出文件。
 */
[[nodiscard]] SZX_LICENSE_API auto issueFile(std::string_view requestPath, const IssueOptions &options,
                                             std::span<const std::uint8_t> issuerPrivateKey,
                                             std::span<const std::uint8_t> issuerPublicKey)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
} // namespace SZX::LICENSE
