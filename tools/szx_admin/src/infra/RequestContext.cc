#include "SzxAdmin/Infra/RequestContext.h"

#include "UUID.hpp"

namespace SZX::ADMIN::INFRA
{
auto isValidRequestId(std::string_view value) noexcept -> bool
{
    if (value.empty() || value.size() > kMaxRequestIdLength)
        return false;
    for (const auto character : value)
    {
        const bool alphaNumeric = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');
        if (!alphaNumeric && character != '-' && character != '_' && character != '.')
            return false;
    }
    return true;
}

auto newRequestId() -> std::string
{
    return SZX::COMMON::Uuid::v4().toString();
}

auto resolveRequestId(std::string_view supplied) -> std::string
{
    return isValidRequestId(supplied) ? std::string{supplied} : newRequestId();
}

auto RequestContext::create(std::string_view supplied) -> RequestContext
{
    return RequestContext{.requestId = resolveRequestId(supplied)};
}
} // namespace SZX::ADMIN::INFRA
