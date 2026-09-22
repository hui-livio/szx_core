#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SZX::ADMIN::AUDIT
{
struct AuditRecord final
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string ip;
    std::string user_agent;
    std::string details_json{"{}"};
    std::string created_at;
};

struct AuditFilter final
{
    std::string from;
    std::string to;
    std::string eventName;
    std::string result;
    std::string actorUserId;
    std::uint32_t limit{20};
    std::uint64_t offset{};
    std::string actorSearch;
    std::string eventSearch;
    std::vector<std::string> eventNames;
};

struct AuditRow final
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string details_json;
    std::string created_at;
    std::string actor_username;
};

struct AuditPage final
{
    std::vector<AuditRow> items;
    std::uint64_t total{};
};

class AuditRepository
{
public:
    virtual ~AuditRepository() = default;

    /** Writes the trusted audit table in the caller's transaction. */
    virtual auto append(const AuditRecord &record) -> SZX::COMMON::Result<void>     = 0;
    virtual auto query(const AuditFilter &filter) -> SZX::COMMON::Result<AuditPage> = 0;
};
} // namespace SZX::ADMIN::AUDIT
