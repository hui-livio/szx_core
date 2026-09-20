#pragma once
/** @file NetworkTypes.h
 * @brief 与底层实现无关的端点、TLS 配置和 HTTP 头类型。
 */
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include "SzxNetwork/NetworkExport.h"

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief IP 地址或 DNS 名称，以及使用主机字节序的端口号。 */
struct Endpoint final
{
    std::string host;       ///< HTTP 监听地址为空时使用回环地址 127.0.0.1。
    std::uint16_t port = 0; ///< 监听端口为 0 时由系统分配临时端口。
};
/** @brief TLS 配置，仅在启用 TLS 时生效。 */
struct TlsOptions final
{
    bool enabled = false;   ///< 是否启用 TLS；启用时要求当前构建支持 TLS。
    std::string certFile;   ///< PEM 证书路径；TLS 服务端必填。
    std::string keyFile;    ///< PEM 私钥路径；TLS 服务端必填。
    std::string caFile;     ///< 可选的对端证书信任库路径。
    bool verifyPeer = true; ///< 当 TLS 后端请求对端证书时校验证书。
};
/** @brief HTTP 头字段名的 ASCII 大小写不敏感排序，不受区域设置影响。 */
struct SZX_NETWORK_API CaseInsensitiveLess final
{
    using is_transparent = void; ///< 支持使用 string_view 等类型进行异构查找。
    /** @brief 按 ASCII 大小写折叠规则比较字段名，不分配内存。
     * @param[in] lhs 左侧字段名。
     * @param[in] rhs 右侧字段名。
     * @return 折叠大小写后 lhs 的字典序小于 rhs 时返回 true。
     */
    auto operator()(std::string_view lhs, std::string_view rhs) const noexcept -> bool;
};
/** @brief 头字段名大小写不敏感；同名重复字段仅保留一个值。 */
using HttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;
} // namespace v1
} // namespace SZX::NETWORK
