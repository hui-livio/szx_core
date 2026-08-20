/**
 * @file UUID.hpp
 * @brief UUID 工具 — 生成和格式化全局唯一标识。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "TimeHelper.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <string_view>

namespace SZX::COMMON
{

// =========================================================
// RNG Layer (统一跨平台)
// =========================================================

class RandomProvider
{
public:
    static void fill(uint8_t *data, size_t len)
    {
        auto &eng = engine();
        auto &mtx = mutex();
        std::lock_guard<std::mutex> lock(mtx);
        size_t i = 0;
        while (i < len)
        {
            uint32_t val     = eng();
            size_t remaining = len - i;
            if (remaining >= 4)
            {
                data[i++] = static_cast<uint8_t>(val & 0xFF);
                data[i++] = static_cast<uint8_t>((val >> 8) & 0xFF);
                data[i++] = static_cast<uint8_t>((val >> 16) & 0xFF);
                data[i++] = static_cast<uint8_t>((val >> 24) & 0xFF);
            }
            else
            {
                for (size_t j = 0; j < remaining; ++j)
                    data[i++] = static_cast<uint8_t>((val >> (j * 8)) & 0xFF);
            }
        }
    }

private:
    static std::mt19937 &engine()
    {
        static std::mt19937 eng = []()
        {
            std::array<int, std::mt19937::state_size> seed_data{};
            std::random_device rd;
            std::generate(seed_data.begin(), seed_data.end(), std::ref(rd));
            std::seed_seq seq(seed_data.begin(), seed_data.end());
            return std::mt19937(seq);
        }();
        return eng;
    }

    static std::mutex &mutex()
    {
        static std::mutex mtx;
        return mtx;
    }
};

// =========================================================
// UUID core
// =========================================================

class Uuid final
{
public:
    std::array<uint8_t, 16> data{};

    // -------------------------
    // factory
    // -------------------------

    static Uuid v4();
    static Uuid v7();

    Uuid() = default;

    static Uuid nil()
    {
        return Uuid();
    }

    bool isNil() const
    {
        for (auto b : data)
            if (b)
                return false;
        return true;
    }

    // -------------------------
    // string
    // -------------------------

    std::string toString() const;
    std::string toStringCompact() const;

    static std::optional<Uuid> parse(std::string_view str);

    // -------------------------
    // compare
    // -------------------------

    auto operator<=>(const Uuid &) const = default;
    bool operator==(const Uuid &) const  = default;

private:
    explicit Uuid(const std::array<uint8_t, 16> &d) :
        data(d)
    {
    }

    void setVersion(uint8_t v)
    {
        data[6] = (data[6] & 0x0F) | (v << 4);
    }

    void setVariant()
    {
        data[8] = (data[8] & 0x3F) | 0x80;
    }
};

// =========================================================
// V7 monotonic state (infra级关键点)
// =========================================================

class UuidV7Clock
{
public:
    struct Tick
    {
        uint64_t ms;
        uint16_t counter;
    };

    static Tick next(uint64_t ms)
    {
        // state = (timestampMs << 16) | counter，打包为 uint64_t 保证 lock-free CAS
        uint64_t key      = ms << 16;
        uint64_t expected = sState.load(std::memory_order_relaxed);
        uint64_t desired;

        do
        {
            uint64_t prevMs = expected >> 16;
            uint16_t prevC  = static_cast<uint16_t>(expected & 0xFFFF);

            if (prevMs == ms)
            {
                // same ms → increment counter
                uint16_t next = static_cast<uint16_t>(prevC + 1);
                if (next != 0)
                    desired = expected + 1; // carry handled by uint64 add
                else
                    desired = (ms + 1) << 16; // overflow → timestampMs + 1
            }
            else if (ms > prevMs || expected == 0)
            {
                // new ms → reset counter
                desired = key;
            }
            else
            {
                // NTP fallback: stay at prev timestamp, increment counter
                uint16_t next = static_cast<uint16_t>(prevC + 1);
                if (next != 0)
                    desired = expected + 1;
                else
                    desired = ((prevMs + 1) << 16);
            }
        } while (!sState.compare_exchange_weak(expected, desired, std::memory_order_relaxed));

        return Tick{.ms = desired >> 16, .counter = static_cast<uint16_t>(desired & 0xFFFF)};
    }

    static uint16_t nextCounter(uint64_t ms)
    {
        return next(ms).counter;
    }

#if defined(SZX_COMMON_TESTING)
    static void ResetForTesting(uint64_t ms = 0, uint16_t counter = 0)
    {
        sState.store((ms << 16) | counter, std::memory_order_relaxed);
    }
#endif

private:
    inline static std::atomic<uint64_t> sState{0};
};

// =========================================================
// implementation
// =========================================================

inline Uuid Uuid::v4()
{
    std::array<uint8_t, 16> raw{};
    RandomProvider::fill(raw.data(), raw.size());

    Uuid u(raw);
    u.setVersion(4);
    u.setVariant();
    return u;
}

inline Uuid Uuid::v7()
{
    std::array<uint8_t, 16> raw{};
    RandomProvider::fill(raw.data(), raw.size());

    const auto tick = UuidV7Clock::next(static_cast<uint64_t>(TimeHelper::NowTimestamp()));
    const auto ms   = tick.ms;
    const auto c    = tick.counter;

    // 48-bit timestamp
    raw[0] = (ms >> 40) & 0xFF;
    raw[1] = (ms >> 32) & 0xFF;
    raw[2] = (ms >> 24) & 0xFF;
    raw[3] = (ms >> 16) & 0xFF;
    raw[4] = (ms >> 8) & 0xFF;
    raw[5] = (ms) & 0xFF;

    // sub-ms ordering (RFC-friendly enhancement)
    // rand_a (12bit) = counter, 不破坏 version/variant 结构。
    // raw[6] 高4位稍后由 setVersion(7) 覆写为 0x7，此处直接赋值。
    raw[6] = ((c >> 8) & 0x0F);
    raw[7] = c & 0xFF;

    Uuid u(raw);
    u.setVersion(7);
    u.setVariant();
    return u;
}

// =========================================================
// hex tables (infra hot path优化)
// =========================================================

namespace detail
{
static constexpr char HEX[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

constexpr uint8_t hexVal(char c) noexcept
{
    if (c >= '0' && c <= '9')
        return static_cast<uint8_t>(c - '0');
    if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t>(c - 'a' + 10);
    return 0xFF;
}
} // namespace detail

// =========================================================
// toString
// =========================================================

inline std::string Uuid::toString() const
{
    std::string s(36, '\0');
    int p = 0;

    auto emit = [&](uint8_t b)
    {
        s[p++] = detail::HEX[b >> 4];
        s[p++] = detail::HEX[b & 0x0F];
    };

    emit(data[0]);
    emit(data[1]);
    emit(data[2]);
    emit(data[3]);
    s[p++] = '-';

    emit(data[4]);
    emit(data[5]);
    s[p++] = '-';

    emit(data[6]);
    emit(data[7]);
    s[p++] = '-';

    emit(data[8]);
    emit(data[9]);
    s[p++] = '-';

    emit(data[10]);
    emit(data[11]);
    for (int i = 12; i < 16; ++i)
        emit(data[i]);

    return s;
}

inline std::string Uuid::toStringCompact() const
{
    std::string s(32, '\0');

    for (int i = 0; i < 16; ++i)
    {
        s[i * 2]     = detail::HEX[data[i] >> 4];
        s[i * 2 + 1] = detail::HEX[data[i] & 0x0F];
    }

    return s;
}

// =========================================================
// parse (infra-safe)
// =========================================================

inline std::optional<Uuid> Uuid::parse(std::string_view str)
{
    if (str.size() == 38 && str.front() == '{' && str.back() == '}')
        str = str.substr(1, 36);

    if (str.size() != 32 && str.size() != 36)
        return std::nullopt;

    // 36-char 格式：提前校验连字符位置
    if (str.size() == 36)
    {
        if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
            return std::nullopt;
    }

    std::array<uint8_t, 16> raw{};
    int di = 0;

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '-')
            continue; // 已在上面校验过位置

        if (di >= 16)
            return std::nullopt;

        uint8_t hi = detail::hexVal(str[i]);
        if (hi == 0xFF)
            return std::nullopt;
        ++i;

        if (i >= str.size())
            return std::nullopt;

        uint8_t lo = detail::hexVal(str[i]);
        if (lo == 0xFF)
            return std::nullopt;

        raw[di++] = (hi << 4) | lo;
    }

    if (di != 16)
        return std::nullopt;
    return Uuid(raw);
}

// =========================================================
// stream
// =========================================================

inline std::ostream &operator<<(std::ostream &os, const Uuid &u)
{
    return os << u.toString();
}

inline std::string operator+(const Uuid &lhs, std::string_view rhs)
{
    std::string result  = lhs.toString();
    result             += rhs;
    return result;
}

inline std::string operator+(std::string_view lhs, const Uuid &rhs)
{
    std::string result(lhs);
    result += rhs.toString();
    return result;
}

inline std::string operator+(const Uuid &lhs, const char *rhs)
{
    return lhs + std::string_view(rhs == nullptr ? "" : rhs);
}

inline std::string operator+(const char *lhs, const Uuid &rhs)
{
    return std::string_view(lhs == nullptr ? "" : lhs) + rhs;
}

inline std::string operator+(const Uuid &lhs, const std::string &rhs)
{
    return lhs + std::string_view(rhs);
}

inline std::string operator+(const std::string &lhs, const Uuid &rhs)
{
    return std::string_view(lhs) + rhs;
}

inline std::string &operator+=(std::string &lhs, const Uuid &rhs)
{
    lhs += rhs.toString();
    return lhs;
}

} // namespace SZX::COMMON

// =========================================================
// std::format 支持
// =========================================================

template <>
struct std::formatter<SZX::COMMON::Uuid>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(const SZX::COMMON::Uuid &uid, std::format_context &ctx) const
    {
        return std::format_to(ctx.out(), "{}", uid.toString());
    }
};
