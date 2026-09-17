#pragma once

#include "SzxAdmin/Infra/HttpTransport.h"

#include <string_view>

namespace SZX::ADMIN::INFRA
{
class DatabasePool;
class HttpTransport;
} // namespace SZX::ADMIN::INFRA

namespace SZX::ADMIN
{
/** Register service endpoints before starting transport. Database must outlive transport callbacks. */
[[nodiscard]] auto registerRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database) -> SZX::COMMON::Result<void>;

/** Install reserved GET routes through the transport adapter during its initialization. */
[[nodiscard]] auto
registerHealthRoutes(std::string_view apiPrefix,
                     const std::function<SZX::COMMON::Result<void>(std::string, INFRA::HttpHandler)> &registerGet)
    -> SZX::COMMON::Result<void>;
} // namespace SZX::ADMIN
