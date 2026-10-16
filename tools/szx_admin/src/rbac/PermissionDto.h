#pragma once
#include "PermissionRepository.h"
#include "Serializer.hpp"

namespace SZX::ADMIN::RBAC
{
struct PermissionDto final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::string module;
    std::string enforcement;
    std::string source;
    std::string status;
    bool required_for_super_admin{};
    std::uint32_t version{};
};
SZX_REFL(PermissionDto, id, code, name, description, module, enforcement, source, status, required_for_super_admin, version);
struct PermissionResponse final
{
    PermissionDto data;
    std::string request_id;
};
SZX_REFL(PermissionResponse, data, request_id);
struct PermissionListDto final
{
    std::vector<PermissionDto> items;
};
SZX_REFL(PermissionListDto, items);
struct PermissionListResponse final
{
    PermissionListDto data;
    std::string request_id;
};
SZX_REFL(PermissionListResponse, data, request_id);
} // namespace SZX::ADMIN::RBAC
