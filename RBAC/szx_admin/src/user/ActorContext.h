#pragma once

#include <string>

namespace SZX::ADMIN::USER
{
/** Authenticated request context. The peer address always comes from the transport. */
struct ActorContext final
{
    std::string accessToken;
    std::string requestId;
    std::string peerAddress;
    std::string userAgent;
};
} // namespace SZX::ADMIN::USER
