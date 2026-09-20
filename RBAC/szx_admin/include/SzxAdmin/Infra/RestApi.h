#pragma once

#include "Serializer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace SZX::ADMIN::INFRA
{
struct PageRequest final
{
    std::uint32_t page{1};
    std::uint32_t pageSize{20};

    [[nodiscard]] auto offset() const noexcept -> std::uint64_t;
};

/** Protocol helpers for admin controllers. Failure results carry only admin codes;
 * no parser, database, or credential diagnostics are returned to the client. */
class RestApi final
{
public:
    /** Require Content-Type application/json (optional UTF-8 charset) and a JSON object. */
    [[nodiscard]] static auto jsonObject(const HttpRequest &request) -> SZX::COMMON::Result<SZX::COMMON::Json>;
    /** Reject fields outside an endpoint's allowlist. Required fields are checked by its controller. */
    [[nodiscard]] static auto allowedFields(const SZX::COMMON::Json &object, std::span<const std::string_view> fields)
        -> SZX::COMMON::Result<void>;
    /** Strict positive decimal pagination; page_size must not exceed 100. */
    [[nodiscard]] static auto pagination(const HttpRequest &request) -> SZX::COMMON::Result<PageRequest>;
    /** Require a positive database INT version, bare or a single quoted strong ETag. */
    [[nodiscard]] static auto ifMatch(const HttpRequest &request) -> SZX::COMMON::Result<std::uint32_t>;
    /** Parse a bounded Bearer credential. Cryptographic/session verification belongs to auth. */
    [[nodiscard]] static auto bearerToken(const HttpRequest &request) -> SZX::COMMON::Result<std::string>;

    /** Build a data/request_id envelope; supported status codes are 200, 201, and 202. */
    [[nodiscard]] static auto success(SZX::COMMON::Json data, std::string_view requestId, int statusCode = 200) -> HttpResponse;
    /** Empty 204 response. */
    [[nodiscard]] static auto noContent(std::string_view requestId) -> HttpResponse;
    /** Build a stable error envelope with empty details and a safe default message. */
    [[nodiscard]] static auto error(ErrorCode code, std::string_view requestId) -> HttpResponse;
};
} // namespace SZX::ADMIN::INFRA
