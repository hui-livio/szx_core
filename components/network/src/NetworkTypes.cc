#include "SzxNetwork/NetworkTypes.h"
#include <algorithm>

namespace SZX::NETWORK::v1
{
auto CaseInsensitiveLess::operator()(std::string_view lhs, std::string_view rhs) const noexcept -> bool
{
    const auto fold = [](unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    };
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                                        [&](unsigned char a, unsigned char b) { return fold(a) < fold(b); });
}
} // namespace SZX::NETWORK::v1
