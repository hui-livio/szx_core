#pragma once
#include "RoleRepository.h"
#include "Serializer.hpp"

namespace SZX::ADMIN::RBAC
{
struct RoleDto final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::optional<std::string> system_key;
    std::string status;
    std::uint32_t version{};
    std::uint32_t authz_version{};
    std::uint64_t user_count{};
    std::uint64_t permission_count{};
    bool manageable{};
};
SZX_REFL(RoleDto, id, code, name, description, system_key, status, version, authz_version, user_count, permission_count,
         manageable);
struct RoleResponse final
{
    RoleDto data;
    std::string request_id;
};
SZX_REFL(RoleResponse, data, request_id);
struct RoleListDto final
{
    std::vector<RoleDto> items;
};
SZX_REFL(RoleListDto, items);
struct RoleListResponse final
{
    RoleListDto data;
    std::string request_id;
};
SZX_REFL(RoleListResponse, data, request_id);
} // namespace SZX::ADMIN::RBAC
