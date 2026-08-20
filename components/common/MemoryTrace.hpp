/**
 * @file MemoryTrace.hpp
 * @brief Lightweight opt-in process memory sampling for diagnostic logs.
 */

#pragma once

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#ifdef _WIN32
    #include <windows.h>

    #include <psapi.h>
#endif

namespace SZX::COMMON::MemoryTrace
{

struct ProcessMemorySnapshot
{
    bool available{false};
    std::uint64_t workingSetBytes{0};
    std::uint64_t privateBytes{0};
    std::uint64_t peakWorkingSetBytes{0};
};

inline auto configEnabled() -> std::atomic_bool &
{
    static std::atomic_bool enabled{false};
    return enabled;
}

inline void setConfigEnabled(bool enabled)
{
    configEnabled().store(enabled, std::memory_order_relaxed);
}

inline auto environmentEnabled() -> bool
{
#ifdef _WIN32
    char *rawValue     = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&rawValue, &length, "SZX_PCB_MEMORY_TRACE") != 0 || rawValue == nullptr)
        return false;
    std::string_view text(rawValue, length > 0 ? length - 1 : 0);
#else
    const char *value = std::getenv("SZX_PCB_MEMORY_TRACE");
    if (value == nullptr)
        return false;
    std::string_view text(value);
#endif
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    const bool enabled = text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
#ifdef _WIN32
    std::free(rawValue);
#endif
    return enabled;
}

[[nodiscard]] inline auto isEnabled() -> bool
{
    return configEnabled().load(std::memory_order_relaxed) || environmentEnabled();
}

[[nodiscard]] inline auto currentProcess() -> ProcessMemorySnapshot
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters)) ==
        FALSE)
        return {};

    return {.available           = true,
            .workingSetBytes     = static_cast<std::uint64_t>(counters.WorkingSetSize),
            .privateBytes        = static_cast<std::uint64_t>(counters.PrivateUsage),
            .peakWorkingSetBytes = static_cast<std::uint64_t>(counters.PeakWorkingSetSize)};
#else
    return {};
#endif
}

} // namespace SZX::COMMON::MemoryTrace
