#pragma once

#include "BehaviorEventRepository.h"
#include "BehaviorEventWriter.h"
#include "../user/ActorContext.h"
#include "../rbac/AuthorizationService.h"

#include "SzxAdmin/Infra/Database.h"

#include <string>

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::TELEMETRY
{
struct BehaviorEvent final
{
    std::string eventName;
    std::string result;
    std::string detailsJson;
};

struct BehaviorEventQuery final
{
    std::string from;
    std::string to;
    std::string eventName;
    std::string result;
    std::string userId;
    std::uint32_t page{1};
    std::uint32_t pageSize{20};
    std::string userSearch;
    std::string eventSearch;
};

class BehaviorEventService final
{
public:
    BehaviorEventService(INFRA::DatabasePool &database, AUTH::LoginService &auth, BehaviorEventWriter &writer);

    [[nodiscard]] auto record(const USER::ActorContext &actor, const BehaviorEvent &event)
        -> SZX::COMMON::Result<std::string>;
    [[nodiscard]] auto list(const USER::ActorContext &actor, const BehaviorEventQuery &query)
        -> SZX::COMMON::Result<BehaviorEventPage>;
    [[nodiscard]] auto retryAfterSeconds() const noexcept -> std::uint32_t
    {
        return writer_.config().retryAfterSeconds;
    }

private:
    INFRA::DatabasePool &database_;
    RBAC::AuthorizationService authorization_;
    BehaviorEventWriter &writer_;
};
} // namespace SZX::ADMIN::TELEMETRY
