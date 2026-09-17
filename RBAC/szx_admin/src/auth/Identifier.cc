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
        const auto length =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, trimmed.data(), static_cast<int>(trimmed.size()), nullptr, 0);
        if (length <= 0)
            return invalid();
        std::wstring wide(length, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, trimmed.data(), static_cast<int>(trimmed.size()), wide.data(),
                                 length))
            return invalid();
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
        const auto bytes =
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), count, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
            return invalid();
        result.resize(bytes);
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), count, result.data(), bytes, nullptr, nullptr))
            return invalid();
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
    } catch (...)
    {
        return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN::AUTH
