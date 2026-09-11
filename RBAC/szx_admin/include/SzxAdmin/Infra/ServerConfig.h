#pragma once

#include "Result.hpp"
#include "Serializer.hpp"

#include <cstdint>
#include <string>

namespace SZX::ADMIN::INFRA
{
struct ServerConfig final
{
    std::string host{"127.0.0.1"};
    std::uint16_t port{8080};
    std::uint16_t workerThreads{};
    std::uint32_t maxRequestBodyBytes{1024U * 1024U};
    std::uint32_t maxConnections{1024};
    std::string apiPrefix{"/api/v1"};
    bool accessLog{};
    std::string staticRoot{"web"};
    std::string staticHomePage{"index.html"};
    bool spaFallback{true};

    [[nodiscard]] auto validate() const -> SZX::COMMON::Result<void>;
};
SZX_REFL(ServerConfig, host, port, workerThreads, maxRequestBodyBytes, maxConnections, apiPrefix, accessLog, staticRoot,
         staticHomePage, spaFallback);
} // namespace SZX::ADMIN::INFRA
