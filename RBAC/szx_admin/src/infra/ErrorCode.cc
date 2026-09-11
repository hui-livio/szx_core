#include "SzxAdmin/Infra/ErrorCode.h"

namespace SZX::ADMIN::INFRA
{
auto errorCodeName(ErrorCode code) noexcept -> std::string_view
{
    switch (code)
    {
    case ErrorCode::AuthRequired:
        return "AUTH_REQUIRED";
    case ErrorCode::TokenExpired:
        return "TOKEN_EXPIRED";
    case ErrorCode::TokenReplayed:
        return "TOKEN_REPLAYED";
    case ErrorCode::AuthInvalidCredentials:
        return "AUTH_INVALID_CREDENTIALS";
    case ErrorCode::Forbidden:
        return "FORBIDDEN";
    case ErrorCode::ResourceNotFound:
        return "RESOURCE_NOT_FOUND";
    case ErrorCode::ValidationError:
        return "VALIDATION_ERROR";
    case ErrorCode::UserIdentityOneOfRequired:
        return "USER_IDENTITY_ONE_OF_REQUIRED";
    case ErrorCode::UserIdentityConflict:
        return "USER_IDENTITY_CONFLICT";
    case ErrorCode::ConflictVersion:
        return "CONFLICT_VERSION";
    case ErrorCode::LastSuperAdminProtected:
        return "LAST_SUPER_ADMIN_PROTECTED";
    case ErrorCode::RateLimited:
        return "RATE_LIMITED";
    case ErrorCode::AuditWriteFailed:
        return "AUDIT_WRITE_FAILED";
    case ErrorCode::RequestIdInvalid:
        return "REQUEST_ID_INVALID";
    case ErrorCode::RouteConflict:
        return "ROUTE_CONFLICT";
    case ErrorCode::PayloadTooLarge:
        return "PAYLOAD_TOO_LARGE";
    case ErrorCode::ServiceUnavailable:
        return "SERVICE_UNAVAILABLE";
    case ErrorCode::TransportFailure:
        return "TRANSPORT_FAILURE";
    case ErrorCode::InvalidConfiguration:
        return "INVALID_CONFIGURATION";
    case ErrorCode::MethodNotAllowed:
        return "METHOD_NOT_ALLOWED";
    case ErrorCode::DatabaseUnavailable:
        return "DATABASE_UNAVAILABLE";
    case ErrorCode::DatabaseQueryFailed:
        return "DATABASE_QUERY_FAILED";
    case ErrorCode::DatabaseTransactionFailed:
        return "DATABASE_TRANSACTION_FAILED";
    case ErrorCode::MigrationFailed:
        return "MIGRATION_FAILED";
    case ErrorCode::MigrationConflict:
        return "MIGRATION_CONFLICT";
    case ErrorCode::InternalFailure:
        return "INTERNAL_FAILURE";
    }
    return "INTERNAL_FAILURE";
}

auto errorCodeHttpStatus(ErrorCode code) noexcept -> int
{
    switch (code)
    {
    case ErrorCode::AuthRequired:
    case ErrorCode::TokenExpired:
    case ErrorCode::AuthInvalidCredentials:
        return 401;
    case ErrorCode::Forbidden:
        return 403;
    case ErrorCode::ResourceNotFound:
        return 404;
    case ErrorCode::ConflictVersion:
    case ErrorCode::UserIdentityConflict:
    case ErrorCode::LastSuperAdminProtected:
    case ErrorCode::TokenReplayed:
    case ErrorCode::RouteConflict:
        return 409;
    case ErrorCode::PayloadTooLarge:
        return 413;
    case ErrorCode::ValidationError:
    case ErrorCode::UserIdentityOneOfRequired:
    case ErrorCode::RequestIdInvalid:
    case ErrorCode::InvalidConfiguration:
        return 422;
    case ErrorCode::MethodNotAllowed:
        return 405;
    case ErrorCode::RateLimited:
        return 429;
    case ErrorCode::ServiceUnavailable:
    case ErrorCode::DatabaseUnavailable:
        return 503;
    case ErrorCode::AuditWriteFailed:
    case ErrorCode::TransportFailure:
    case ErrorCode::DatabaseQueryFailed:
    case ErrorCode::DatabaseTransactionFailed:
    case ErrorCode::MigrationFailed:
    case ErrorCode::MigrationConflict:
    case ErrorCode::InternalFailure:
        return 500;
    }
    return 500;
}

auto defaultErrorMessage(ErrorCode code) noexcept -> std::string_view
{
    switch (code)
    {
    case ErrorCode::AuthRequired:
        return "Authentication is required.";
    case ErrorCode::TokenExpired:
        return "The access token has expired.";
    case ErrorCode::TokenReplayed:
        return "The refresh token was replayed.";
    case ErrorCode::AuthInvalidCredentials:
        return "The supplied credentials are invalid.";
    case ErrorCode::Forbidden:
        return "The current user is not allowed to perform this operation.";
    case ErrorCode::ResourceNotFound:
        return "The requested resource was not found.";
    case ErrorCode::ValidationError:
        return "The request is invalid.";
    case ErrorCode::UserIdentityOneOfRequired:
        return "Exactly one user identity is required.";
    case ErrorCode::UserIdentityConflict:
        return "The user identity conflicts with an existing user.";
    case ErrorCode::ConflictVersion:
        return "The resource was changed by another request.";
    case ErrorCode::LastSuperAdminProtected:
        return "The last active super admin cannot be removed.";
    case ErrorCode::RateLimited:
        return "Too many requests.";
    case ErrorCode::AuditWriteFailed:
        return "The security audit could not be recorded.";
    case ErrorCode::RequestIdInvalid:
        return "The request ID is invalid.";
    case ErrorCode::RouteConflict:
        return "The route is already registered.";
    case ErrorCode::PayloadTooLarge:
        return "The request body is too large.";
    case ErrorCode::ServiceUnavailable:
        return "The service is temporarily unavailable.";
    case ErrorCode::TransportFailure:
        return "The HTTP transport operation failed.";
    case ErrorCode::InvalidConfiguration:
        return "The service configuration is invalid.";
    case ErrorCode::MethodNotAllowed:
        return "The HTTP method is not allowed for this resource.";
    case ErrorCode::DatabaseUnavailable:
        return "The database is temporarily unavailable.";
    case ErrorCode::DatabaseQueryFailed:
        return "The database operation failed.";
    case ErrorCode::DatabaseTransactionFailed:
        return "The database transaction failed.";
    case ErrorCode::MigrationFailed:
        return "The database migration failed.";
    case ErrorCode::MigrationConflict:
        return "The database migration conflicts with the recorded schema.";
    case ErrorCode::InternalFailure:
        return "An internal server error occurred.";
    }
    return "An internal server error occurred.";
}
} // namespace SZX::ADMIN::INFRA
