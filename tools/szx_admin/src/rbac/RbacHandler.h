#pragma once

#include "SzxAdmin/Infra/HttpTransport.h"

namespace SZX::ADMIN::INFRA
{
class DatabasePool;
}

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::RBAC
{
[[nodiscard]] auto registerRbacRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database,
                                      AUTH::LoginService &login) -> SZX::COMMON::Result<void>;
} // namespace SZX::ADMIN::RBAC
