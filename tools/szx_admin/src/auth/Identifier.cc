#include "LoginService.h"
#include "StringHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unicode/locid.h>
    #include <unicode/normalizer2.h>
    #include <unicode/unistr.h>
#endif

namespace SZX::ADMIN::AUTH
{
auto canonicalIdentifier(std::string_view input, bool username) -> SZX::COMMON::Result<std::string>
{
    using SZX::COMMON::Result;
    const auto invalid = []
    {
        return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::ValidationError), {});
    };
    try
    {
        if (input.size() > 512)
            return invalid();
        const auto trimmed = SZX::COMMON::StringHelper::Strip(input);
        if (trimmed.empty())
            return invalid();
        for (const unsigned char c : trimmed)
            if (c < 32 || c == 127)
                return invalid();
        std::string result;
#ifdef _WIN32
        auto wide         = SZX::COMMON::StringHelper::fromutf8(trimmed);
        const auto length = static_cast<int>(wide.size());
        if (username)
        {
            const auto size =
                LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), length, nullptr, 0, nullptr, nullptr, 0);
            if (size <= 0)
                return invalid();
            std::wstring lower(size, L'\0');
            if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), length, lower.data(), size, nullptr, nullptr,
                               0))
                return invalid();
            wide = std::move(lower);
        }
        const auto capacity = NormalizeString(NormalizationC, wide.data(), static_cast<int>(wide.size()), nullptr, 0);
        if (capacity <= 0)
            return invalid();
        std::wstring normalized(capacity, L'\0');
        const auto count =
            NormalizeString(NormalizationC, wide.data(), static_cast<int>(wide.size()), normalized.data(), capacity);
        if (count <= 0)
            return invalid();
        normalized.resize(count);
        result = SZX::COMMON::StringHelper::toutf8(normalized);
#else
        auto wide = icu::UnicodeString::fromUTF8(trimmed);
        std::string roundTrip;
        wide.toUTF8String(roundTrip);
        if (roundTrip != trimmed)
            return invalid();
        if (username)
            wide.toLower(icu::Locale::getRoot());
        UErrorCode status      = U_ZERO_ERROR;
        const auto *normalizer = icu::Normalizer2::getNFCInstance(status);
        icu::UnicodeString normalized;
        if (U_FAILURE(status))
            return invalid();
        normalizer->normalize(wide, normalized, status);
        if (U_FAILURE(status))
            return invalid();
        normalized.toUTF8String(result);
#endif
        std::size_t characters{};
        for (const unsigned char c : result)
            if ((c & 0xc0) != 0x80)
                ++characters;
        if (characters == 0 || characters > 64)
            return invalid();
        return Result<std::string>::Success(std::move(result));
    } catch (const std::invalid_argument &)
    {
        return invalid();
    } catch (...)
    {
        return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}

auto normalizeLoginIdentifier(std::string_view input, IdentifierType type) -> SZX::COMMON::Result<LoginIdentifier>
{
    using SZX::COMMON::Result;
    const auto display = canonicalIdentifier(input, false);
    if (!display.ok)
        return Result<LoginIdentifier>::Failure(display.error, {});
    // V1.2 gives UID and username one uniqueness domain. Reuse the same
    // invariant-lowercase/Unicode-normalization path for both types; the
    // legacy canonicalIdentifier(uid, false) remains available only for the
    // pre-migration storage adapter.
    const auto canonical = canonicalIdentifier(input, true);
    if (!canonical.ok)
        return Result<LoginIdentifier>::Failure(canonical.error, {});
    return Result<LoginIdentifier>::Success(
        LoginIdentifier{.displayValue = std::move(display.value), .canonicalValue = std::move(canonical.value), .type = type});
}
} // namespace SZX::ADMIN::AUTH
