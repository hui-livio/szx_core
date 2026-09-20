#pragma once

#include "Result.hpp"

#include <string_view>

namespace SZX::ADMIN::INFRA
{
struct DatabaseConfig;

namespace DETAIL
{
struct InitialAdministratorDefinition final
{
    std::string_view username;
    std::string_view password;
    std::string_view displayName;
    std::string_view role;
    std::string_view status;
    bool forcePasswordChange;
};

[[nodiscard]] auto initialAdministrator() noexcept -> const InitialAdministratorDefinition &;

// Private initialization entry used before opening the database connection pool.
[[nodiscard]] auto ensureDatabase(const DatabaseConfig &config) -> SZX::COMMON::Result<void>;
} // namespace DETAIL
} // namespace SZX::ADMIN::INFRA
