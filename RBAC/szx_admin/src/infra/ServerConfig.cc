#include "SzxAdmin/Infra/ServerConfig.h"

#include "SzxAdmin/Infra/ErrorCode.h"

namespace SZX::ADMIN::INFRA
{
namespace
{
auto failure(const char *message) -> SZX::COMMON::Result<void>
{
    return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InvalidConfiguration), message);
}
} // namespace

auto ServerConfig::validate() const -> SZX::COMMON::Result<void>
{
    if (host.empty() || host.size() > 253)
        return failure("HTTP host 配置无效。");
    if (port == 0)
        return failure("HTTP port 配置无效。");
    if (workerThreads > 64)
        return failure("HTTP worker thread 配置无效。");
    if (maxRequestBodyBytes == 0 || maxRequestBodyBytes > 64U * 1024U * 1024U)
        return failure("HTTP request body 上限无效。");
    if (maxConnections == 0)
        return failure("HTTP connection 上限无效。");
    if (apiPrefix.empty() || apiPrefix.front() != '/' || apiPrefix.find('?') != std::string::npos ||
        apiPrefix.find('#') != std::string::npos || (apiPrefix.size() > 1 && apiPrefix.back() == '/'))
        return failure("HTTP API prefix 配置无效。");
    return SZX::COMMON::Result<void>::Success();
}

} // namespace SZX::ADMIN::INFRA
