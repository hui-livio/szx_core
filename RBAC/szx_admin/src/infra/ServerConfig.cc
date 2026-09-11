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
        return failure("Invalid HTTP host configuration.");
    if (port == 0)
        return failure("Invalid HTTP port configuration.");
    if (workerThreads > 64)
        return failure("Invalid HTTP worker thread configuration.");
    if (maxRequestBodyBytes == 0 || maxRequestBodyBytes > 64U * 1024U * 1024U)
        return failure("Invalid HTTP request body limit.");
    if (maxConnections == 0)
        return failure("Invalid HTTP connection limit.");
    if (apiPrefix.empty() || apiPrefix.front() != '/' || apiPrefix.find('?') != std::string::npos ||
        apiPrefix.find('#') != std::string::npos || (apiPrefix.size() > 1 && apiPrefix.back() == '/'))
        return failure("Invalid HTTP API prefix configuration.");
    return SZX::COMMON::Result<void>::Success();
}

} // namespace SZX::ADMIN::INFRA
