#pragma once

#include "SzxAdmin/Infra/HttpTransport.h"

namespace SZX::ADMIN::INFRA
{
class DatabasePool;
} // namespace SZX::ADMIN::INFRA

namespace SZX::ADMIN::AUTH
{
class LoginService;
struct LoginConfig;
} // namespace SZX::ADMIN::AUTH

namespace SZX::ADMIN::USER
{
/** Register user management endpoints. Requires login service for access token verification. */
[[nodiscard]] auto registerUserRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database, AUTH::LoginService &login)
    -> SZX::COMMON::Result<void>;
} // namespace SZX::ADMIN::USER
