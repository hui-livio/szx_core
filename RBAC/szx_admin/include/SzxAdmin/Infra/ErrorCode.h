#pragma once

#include <string>
#include <string_view>

namespace SZX::ADMIN::INFRA
{
enum class ErrorCode : int
{
    AuthRequired              = 2200,
    TokenExpired              = 2201,
    TokenReplayed             = 2202,
    AuthInvalidCredentials    = 2203,
    Forbidden                 = 2204,
    ResourceNotFound          = 2205,
    ValidationError           = 2206,
    UserIdentityOneOfRequired = 2207,
    UserIdentityConflict      = 2208,
    ConflictVersion           = 2209,
    LastSuperAdminProtected   = 2210,
    RateLimited               = 2211,
    AuditWriteFailed          = 2212,
    RequestIdInvalid          = 2213,
    RouteConflict             = 2214,
    PayloadTooLarge           = 2215,
    ServiceUnavailable        = 2216,
    TransportFailure          = 2217,
    InvalidConfiguration      = 2218,
    MethodNotAllowed          = 2219,
    DatabaseUnavailable       = 2220,
    DatabaseQueryFailed       = 2221,
    DatabaseTransactionFailed = 2222,
    MigrationFailed           = 2223,
    MigrationConflict         = 2224,
    BehaviorEventQueueFull    = 2225,
    BehaviorEventQueueClosed  = 2226,
    InternalFailure           = 2299,
};

[[nodiscard]] auto errorCodeName(ErrorCode code) noexcept -> std::string_view;
[[nodiscard]] auto errorCodeHttpStatus(ErrorCode code) noexcept -> int;
[[nodiscard]] auto defaultErrorMessage(ErrorCode code) noexcept -> std::string_view;

struct ApiError final
{
    ErrorCode code;
    std::string message;
};
} // namespace SZX::ADMIN::INFRA
