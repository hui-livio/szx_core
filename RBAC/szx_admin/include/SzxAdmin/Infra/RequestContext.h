#pragma once

#include <string>
#include <string_view>

namespace SZX::ADMIN::INFRA
{
inline constexpr std::string_view kRequestIdHeader = "X-Request-ID";
inline constexpr std::size_t kMaxRequestIdLength   = 128;

[[nodiscard]] auto isValidRequestId(std::string_view value) noexcept -> bool;
[[nodiscard]] auto newRequestId() -> std::string;
[[nodiscard]] auto resolveRequestId(std::string_view supplied) -> std::string;

struct RequestContext final
{
    std::string requestId;

    [[nodiscard]] static auto create(std::string_view supplied = {}) -> RequestContext;
};
} // namespace SZX::ADMIN::INFRA
