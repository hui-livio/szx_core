#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::RBAC
{
struct PermissionRow final
{
    std::string id;
    std::string code;
    std::string name;
    std::string module;
    std::string enforcement;
    std::string source;
    std::string status;
    bool requiredForSuperAdmin{};
    std::uint32_t version{};
};

/** Runtime permission catalog reads; write behavior belongs to PermissionService. */
class PermissionRepository
{
public:
    virtual ~PermissionRepository() = default;

    virtual auto findById(std::string_view id, bool lock = false)
        -> SZX::COMMON::Result<std::optional<PermissionRow>> = 0;
    virtual auto findByCode(std::string_view code, bool lock = false)
        -> SZX::COMMON::Result<std::optional<PermissionRow>> = 0;
    virtual auto listActiveCodes(std::string_view roleCode)
        -> SZX::COMMON::Result<std::vector<std::string>> = 0;
};
} // namespace SZX::ADMIN::RBAC
