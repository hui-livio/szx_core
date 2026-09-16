#pragma once
/** @file NetworkTypes.h
 * @brief Backend-independent endpoint, TLS and HTTP header types.
 */
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include "SzxNetwork/NetworkExport.h"

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief IP address or DNS name and a host-order port. */
struct Endpoint final
{
    std::string host;       ///< Empty HTTP listen host means loopback (127.0.0.1).
    std::uint16_t port = 0; ///< Zero selects an ephemeral listening port.
};
/** @brief TLS configuration, interpreted only when enabled. */
struct TlsOptions final
{
    bool enabled = false;
    std::string certFile;   ///< PEM certificate path; required for a TLS server.
    std::string keyFile;    ///< PEM private key path; required for a TLS server.
    std::string caFile;     ///< Optional peer certificate trust store.
    bool verifyPeer = true; ///< Verify peer certificates when the TLS backend requests them.
};
/** @brief Locale-independent ASCII ordering for HTTP field names. */
struct SZX_NETWORK_API CaseInsensitiveLess final
{
    using is_transparent = void;
    /** @brief Compare ASCII-folded names without allocation. */
    auto operator()(std::string_view lhs, std::string_view rhs) const noexcept -> bool;
};
/** @brief Case-insensitive HTTP headers; repeated names collapse to one value. */
using HttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;
} // namespace v1
} // namespace SZX::NETWORK
