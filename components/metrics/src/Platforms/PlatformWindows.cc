/** @file PlatformWindows.cc
 * @brief Windows 采集器：PDH、GlobalMemoryStatusEx、GetIfTable2、Toolhelp、NVML、WMI。
 * @details 与 Linux 版本实现同一 DETAIL::Collector 接口，公共层不感知差异。
 *          每个子项由上层 try/catch 隔离，这里把系统接口失败转成 UNAVAILABLE，
 *          不抛异常、不返回脏值。
 */

#include "MetricsInternal.h"

#include "StringHelper.hpp"
#include "TimeHelper.hpp"

#include "NvmlLoader.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <comdef.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wbemidl.h>
#include <winioctl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
namespace
{
/** 极简 COM 智能指针：只在本文件内使用，避免引入 WRL 依赖。 */
template <typename T>
class ComPtr final
{
public:
    ComPtr() = default;
    ~ComPtr()
    {
        reset();
    }
    ComPtr(const ComPtr &)                     = delete;
    auto operator=(const ComPtr &) -> ComPtr & = delete;

    [[nodiscard]] auto put() -> T **
    {
        reset();
        return &pointer_;
    }
    [[nodiscard]] auto get() const -> T *
    {
        return pointer_;
    }
    auto operator->() const -> T *
    {
        return pointer_;
    }
    void reset()
    {
        if (pointer_ != nullptr)
            pointer_->Release();
        pointer_ = nullptr;
    }

private:
    T *pointer_{};
};

/** 当前线程的 COM 初始化；线程已初始化（RPC_E_CHANGED_MODE）不算失败。 */
class ComApartment final
{
public:
    ComApartment() :
        result_(::CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
    }
    ~ComApartment()
    {
        if (SUCCEEDED(result_))
            ::CoUninitialize();
    }
    ComApartment(const ComApartment &)                     = delete;
    auto operator=(const ComApartment &) -> ComApartment & = delete;
    [[nodiscard]] auto ok() const -> bool
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_;
};

/** 执行一条 WQL 查询并逐行回调；命名空间不存在或查询失败返回 false。 */
template <typename Callback>
auto queryWmi(const wchar_t *nameSpace, const wchar_t *query, Callback &&onRow) -> bool
{
    const ComApartment apartment;
    if (!apartment.ok())
        return false;
    ComPtr<IWbemLocator> locator;
    if (FAILED(::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(locator.put()))))
        return false;
    ComPtr<IWbemServices> service;
    if (FAILED(locator->ConnectServer(_bstr_t(nameSpace), nullptr, nullptr, nullptr, 0, nullptr, nullptr, service.put())))
        return false;
    if (FAILED(::CoSetProxyBlanket(service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                   RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE)))
        return false;
    ComPtr<IEnumWbemClassObject> rows;
    if (FAILED(service->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                                  rows.put())))
        return false;
    for (unsigned guard = 0; guard < 512; ++guard)
    {
        ComPtr<IWbemClassObject> row;
        ULONG fetched = 0;
        if (rows->Next(500, 1, row.put(), &fetched) != S_OK || fetched == 0)
            break;
        onRow(row.get());
    }
    return true;
}
/** 从 VARIANT 取数值（兼容 UI1/UI2/UI4/UI8/I4/I8/R4/R8）。 */
auto variantNumber(const VARIANT &value) -> std::optional<double>
{
    switch (value.vt)
    {
    case VT_UI1:
        return value.bVal;
    case VT_UI2:
        return value.uiVal;
    case VT_UI4:
        return value.ulVal;
    case VT_UI8:
        return static_cast<double>(value.ullVal);
    case VT_I4:
        return value.lVal;
    case VT_I8:
        return static_cast<double>(value.llVal);
    case VT_R4:
        return value.fltVal;
    case VT_R8:
        return value.dblVal;
    default:
        return std::nullopt;
    }
}
auto variantOf(IWbemClassObject *row, const wchar_t *name) -> std::optional<double>
{
    VARIANT value{};
    ::VariantInit(&value);
    std::optional<double> result;
    if (SUCCEEDED(row->Get(name, 0, &value, nullptr, nullptr)))
        result = variantNumber(value);
    ::VariantClear(&value);
    return result;
}

/** 宽字符转 UTF-8。 */
auto toUtf8(const wchar_t *text) -> std::string
{
    if (text == nullptr)
        return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr) == 0)
        return {};
    result.pop_back();
    return result;
}
auto lowercase(std::string text) -> std::string
{
    return SZX::COMMON::StringHelper::ToLower(text);
}
/** 进程 exe 名去扩展名、转小写。取不到（权限不足 / 已退出）返回空串。 */
auto processNameOfPid(DWORD pid) -> std::string
{
    const HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (handle == nullptr)
        return {};
    std::array<wchar_t, 512> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    std::wstring full;
    if (::QueryFullProcessImageNameW(handle, 0, buffer.data(), &size) != FALSE)
        full.assign(buffer.data(), size);
    ::CloseHandle(handle);
    if (full.empty())
        return {};
    const auto slash        = full.find_last_of(L"\\/");
    const std::wstring file = slash == std::wstring::npos ? full : full.substr(slash + 1);
    const std::wstring stem =
        (file.size() > 4 && file.compare(file.size() - 4, 4, L".exe") == 0) ? file.substr(0, file.size() - 4) : file;
    return lowercase(toUtf8(stem.c_str()));
}
/** 同名进程会带 "#1" 后缀，因此除全等外还接受「前缀 + #」。 */
auto instanceMatches(const std::string &instance, std::string_view name) -> bool
{
    const std::string lower = lowercase(instance);
    if (lower == name)
        return true;
    return lower.size() > name.size() && lower.compare(0, name.size(), name) == 0 && lower[name.size()] == '#';
}
/** 从 GPU 计数器实例名 `pid_1234_luid_..._eng_3_engtype_Cuda` 取 pid。 */
auto pidFromInstance(const std::string &instance, unsigned &pid) -> bool
{
    constexpr std::size_t kPrefix = 4; // "pid_"
    if (instance.rfind("pid_", 0) != 0)
        return false;
    std::uint64_t value = 0;
    std::size_t at      = kPrefix;
    for (; at < instance.size() && instance[at] >= '0' && instance[at] <= '9'; ++at)
    {
        value = value * 10 + static_cast<std::uint64_t>(instance[at] - '0');
        if (value > 0xFFFFFFFFULL)
            return false;
    }
    if (at == kPrefix)
        return false;
    pid = static_cast<unsigned>(value);
    return true;
}
auto cpuBrandString() -> std::string
{
    std::array<int, 4> registers{};
    __cpuid(registers.data(), static_cast<int>(0x80000000U));
    if (static_cast<unsigned>(registers[0]) < 0x80000004U)
        return {};
    std::array<char, 49> name{};
    for (unsigned index = 0; index < 3; ++index)
    {
        __cpuid(registers.data(), static_cast<int>(0x80000002U + index));
        std::memcpy(name.data() + index * 16, registers.data(), 16);
    }
    std::string result(name.data());
    const auto first = result.find_first_not_of(' ');
    return first == std::string::npos ? std::string{} : result.substr(first, result.find_last_not_of(' ') - first + 1);
}
/** 由卷根路径取物理盘的总线与介质类型。 */
auto diskTypeOfVolume(const std::wstring &volume) -> std::string
{
    if (volume.size() < 2 || volume[1] != L':')
        return "未知类型";
    const std::wstring device = std::wstring(L"\\\\.\\") + volume[0] + L":";
    const HANDLE handle =
        ::CreateFileW(device.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return "未知类型";
    VOLUME_DISK_EXTENTS extents{};
    DWORD returned    = 0;
    const bool mapped = ::DeviceIoControl(handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, &extents, sizeof(extents),
                                          &returned, nullptr) != FALSE;
    ::CloseHandle(handle);
    if (!mapped || extents.NumberOfDiskExtents != 1)
        return "未知类型";

    const std::wstring physical = L"\\\\.\\PhysicalDrive" + std::to_wstring(extents.Extents[0].DiskNumber);
    const HANDLE disk =
        ::CreateFileW(physical.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (disk == INVALID_HANDLE_VALUE)
        return "未知类型";
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;
    STORAGE_DEVICE_DESCRIPTOR descriptor{};
    const bool described = ::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor,
                                             sizeof(descriptor), &returned, nullptr) != FALSE;
    query.PropertyId     = StorageDeviceSeekPenaltyProperty;
    DEVICE_SEEK_PENALTY_DESCRIPTOR seek{};
    const bool sought = ::DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &seek, sizeof(seek),
                                          &returned, nullptr) != FALSE;
    ::CloseHandle(disk);

    std::string bus;
    if (described)
    {
        switch (descriptor.BusType)
        {
        case BusTypeNvme:
            bus = "NVMe";
            break;
        case BusTypeSata:
            bus = "SATA";
            break;
        case BusTypeAta:
            bus = "ATA";
            break;
        case BusTypeUsb:
            bus = "USB";
            break;
        case BusTypeRAID:
            bus = "RAID";
            break;
        case BusTypeVirtual:
        case BusTypeFileBackedVirtual:
            bus = "虚拟";
            break;
        default:
            break;
        }
    }
    const std::string media = sought ? (seek.IncursSeekPenalty != 0U ? "HDD" : "SSD") : "未知介质";
    return bus.empty() ? media : media + "(" + bus + ")";
}

/** PDH 查询的 RAII 包装；计数器失效后整体复位，下次调用重新打开。 */
class PdhQuery final
{
public:
    PdhQuery() = default;
    ~PdhQuery()
    {
        reset();
    }
    PdhQuery(const PdhQuery &)                     = delete;
    auto operator=(const PdhQuery &) -> PdhQuery & = delete;

    [[nodiscard]] auto open(const wchar_t *path) -> bool
    {
        if (query_ != nullptr)
            return true;
        if (::PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS)
        {
            query_ = nullptr;
            return false;
        }
        // 必须用英文计数器名：中文系统上本地化名称会让 PdhAddCounterW 直接失败
        if (::PdhAddEnglishCounterW(query_, path, 0, &counter_) != ERROR_SUCCESS)
        {
            reset();
            return false;
        }
        return true;
    }
    void reset() noexcept
    {
        if (query_ != nullptr)
            ::PdhCloseQuery(query_);
        query_   = nullptr;
        counter_ = nullptr;
    }
    [[nodiscard]] auto collect() -> bool
    {
        return query_ != nullptr && ::PdhCollectQueryData(query_) == ERROR_SUCCESS;
    }
    [[nodiscard]] auto value(double &out) const -> bool
    {
        if (counter_ == nullptr)
            return false;
        PDH_FMT_COUNTERVALUE value{};
        if (::PdhGetFormattedCounterValue(counter_, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS)
            return false;
        if (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA)
            return false;
        if (!std::isfinite(value.doubleValue) || value.doubleValue < 0.0 || value.doubleValue > 100.0)
            return false;
        out = value.doubleValue;
        return true;
    }
    [[nodiscard]] auto counter() const -> PDH_HCOUNTER
    {
        return counter_;
    }

private:
    PDH_HQUERY query_{};
    PDH_HCOUNTER counter_{};
};

/** 通配实例计数器的一帧：实例名 → 数值。 */
auto collectWildcard(PDH_HCOUNTER counter) -> std::map<std::string, double>
{
    std::map<std::string, double> result;
    if (counter == nullptr)
        return result;
    DWORD size        = 0;
    DWORD count       = 0;
    PDH_STATUS status = ::PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr);
    for (int attempt = 0; status == PDH_MORE_DATA && attempt < 4; ++attempt)
    {
        std::vector<BYTE> buffer(size);
        auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
        status      = ::PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, items);
        if (status != ERROR_SUCCESS)
            break;
        for (DWORD index = 0; index < count; ++index)
        {
            const auto &item = items[index];
            // 状态在 FmtValue 内部：PDH_FMT_COUNTERVALUE_ITEM_W 只有 { szName, FmtValue }
            if (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
                continue;
            if (!std::isfinite(item.FmtValue.doubleValue) || item.FmtValue.doubleValue < 0.0)
                continue;
            result.emplace(toUtf8(item.szName), item.FmtValue.doubleValue);
        }
        return result;
    }
    return result;
}

/** 一次进程累计值。 */
struct ProcessCounters final
{
    std::uint64_t cpu100ns{};
    std::uint64_t workingSet{};
};

/** Windows 采集器。 */
class WindowsCollector final : public Collector
{
public:
    [[nodiscard]] auto platformName() const -> const char * override
    {
        return "windows";
    }
    [[nodiscard]] auto logicalCpus() const -> std::uint32_t override
    {
        // 与任务管理器口径一致；CPU 组大于 64 核时 hardware_concurrency 会偏小
        const DWORD count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        return count == 0 ? 0U : static_cast<std::uint32_t>(count);
    }
    [[nodiscard]] auto cpuName() const -> std::string override
    {
        return cpuBrandString();
    }
    [[nodiscard]] auto memorySpec() const -> std::string override
    {
        std::map<std::uint64_t, unsigned> modules;
        std::map<unsigned, unsigned> speeds;
        queryWmi(L"ROOT\\CIMV2", L"SELECT Capacity, ConfiguredClockSpeed FROM Win32_PhysicalMemory",
                 [&modules, &speeds](IWbemClassObject *row)
                 {
                     if (const auto capacity = variantOf(row, L"Capacity"); capacity && *capacity > 0)
                         ++modules[static_cast<std::uint64_t>(*capacity)];
                     if (const auto speed = variantOf(row, L"ConfiguredClockSpeed"); speed && *speed > 0)
                         ++speeds[static_cast<unsigned>(*speed)];
                 });
        std::string result;
        for (const auto &[bytes, count] : modules)
        {
            if (!result.empty())
                result += " + ";
            result += std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + "GB × " + std::to_string(count);
        }
        if (!speeds.empty())
        {
            if (!result.empty())
                result += " · ";
            bool first = true;
            for (const auto &[speed, unused] : speeds)
            {
                if (!first)
                    result.push_back('/');
                result += std::to_string(speed);
                first   = false;
            }
            result += " MT/s";
        }
        return result;
    }
    [[nodiscard]] auto cpuTemperature() -> std::optional<double> override
    {
        // Windows 没有标准温度接口；只读 LibreHardwareMonitor / OpenHardwareMonitor 的 WMI
        // 命名空间，且每 30 秒最多尝试一次，避免每条采样都打 WMI
        const auto now = SZX::COMMON::TimeHelper::NowTimestamp();
        if (now < nextTemperatureAt_)
            return lastTemperature_;
        nextTemperatureAt_ = now + 30000;
        lastTemperature_.reset();
        for (const wchar_t *nameSpace : {L"ROOT\\LibreHardwareMonitor", L"ROOT\\OpenHardwareMonitor"})
        {
            std::optional<double> hottest;
            const bool queried =
                queryWmi(nameSpace,
                         L"SELECT Value FROM Sensor WHERE SensorType='Temperature' AND Identifier LIKE '%cpu%' AND "
                         L"NOT Name LIKE '%TjMax%' AND NOT Name LIKE '%Distance%'",
                         [&hottest](IWbemClassObject *row)
                         {
                             if (const auto value = variantOf(row, L"Value");
                                 value && *value >= 0 && *value <= 150 && (!hottest || *value > *hottest))
                                 hottest = *value;
                         });
            if (queried && hottest)
            {
                lastTemperature_ = hottest;
                break;
            }
        }
        return lastTemperature_;
    }
    [[nodiscard]] auto cpu() -> MetricValue override
    {
        MetricValue result;
        if (!cpuQuery_.open(L"\\Processor Information(_Total)\\% Processor Time"))
        {
            cpuQuery_.reset();
            return result;
        }
        if (!cpuQuery_.collect())
        {
            primed_ = false;
            return result;
        }
        // % Processor Time 是两次采集之间的增量比值，首帧必然没有可用值
        if (!std::exchange(primed_, true))
        {
            result.state = SZX_METRICS_METRIC_WARMING_UP;
            return result;
        }
        double usage = 0.0;
        if (!cpuQuery_.value(usage))
        {
            // 计数器失效（如 CPU 热插拔）后必须重新预热，否则会一直读到脏值
            primed_ = false;
            cpuQuery_.reset();
            return result;
        }
        result.state        = SZX_METRICS_METRIC_OK;
        result.present      = SZX_METRICS_FIELD_USAGE;
        result.usagePercent = usage;
        return result;
    }
    [[nodiscard]] auto memory() -> MetricValue override
    {
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        if (::GlobalMemoryStatusEx(&status) == FALSE)
            return {};
        auto result = capacityMetric(status.ullTotalPhys, status.ullAvailPhys, status.ullAvailPhys);
        // 可用内存含可回收缓存，与「从未分配出去」不是一回事，故不下发 free
        result.present   &= ~static_cast<std::uint32_t>(SZX_METRICS_FIELD_FREE);
        result.freeBytes  = 0;
        return result;
    }
    [[nodiscard]] auto disks(const std::vector<std::string> &configured, std::int64_t sampledAt) -> std::vector<DiskInfo> override
    {
        std::vector<std::string> paths;
        if (configured.empty())
        {
            std::array<wchar_t, 512> drives{};
            const DWORD written = ::GetLogicalDriveStringsW(static_cast<DWORD>(drives.size()), drives.data());
            if (written == 0 || written >= drives.size())
                return {};
            for (const wchar_t *drive = drives.data(); *drive != L'\0'; drive += std::wcslen(drive) + 1)
                if (::GetDriveTypeW(drive) == DRIVE_FIXED)
                    paths.push_back(toUtf8(drive));
        }
        else
        {
            paths = configured;
        }
        std::set<std::wstring> seen;
        std::set<std::string> configuredPaths;
        std::vector<DiskInfo> result;
        for (const auto &path : paths)
        {
            if (!configuredPaths.insert(path).second)
                continue;
            if (result.size() >= SZX_METRICS_MAX_DISK_PATHS)
                break;
            const auto native = std::filesystem::path{std::u8string{path.begin(), path.end()}}.wstring();
            std::array<wchar_t, MAX_PATH> volume{};
            DiskInfo disk{path, path, "未知类型", sampledAt, {}};
            if (::GetVolumePathNameW(native.c_str(), volume.data(), static_cast<DWORD>(volume.size())) == FALSE ||
                ::GetDriveTypeW(volume.data()) != DRIVE_FIXED)
            {
                result.push_back(std::move(disk));
                continue;
            }
            if (!seen.insert(volume.data()).second)
                continue;
            disk.id = disk.path = toUtf8(volume.data());
            disk.type           = diskTypeOfVolume(volume.data());
            ULARGE_INTEGER available{};
            ULARGE_INTEGER total{};
            ULARGE_INTEGER free{};
            if (::GetDiskFreeSpaceExW(volume.data(), &available, &total, &free) != FALSE)
                disk.capacity = capacityMetric(total.QuadPart, free.QuadPart, available.QuadPart);
            result.push_back(std::move(disk));
        }
        return result;
    }
    [[nodiscard]] auto networks(std::int64_t sampledAt) -> std::vector<NetworkInfo> override
    {
        const auto now     = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double>(now - networkAt_).count();
        std::vector<unsigned char> buffer(16384);
        ULONG bytes  = static_cast<ULONG>(buffer.size());
        ULONG status = ERROR_BUFFER_OVERFLOW;
        for (unsigned attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; ++attempt)
        {
            buffer.resize(bytes);
            status = ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &bytes);
        }
        if (status != NO_ERROR)
            return {};

        std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> current;
        std::vector<NetworkInfo> result;
        for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
        {
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL)
                continue;
            MIB_IF_ROW2 row{};
            row.InterfaceIndex = adapter->IfIndex;
            if (::GetIfEntry2(&row) != NO_ERROR || row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;

            NetworkInfo nic;
            nic.id          = std::to_string(row.InterfaceLuid.Value);
            nic.name        = toUtf8(row.Alias);
            nic.description = toUtf8(row.Description);
            if (nic.name.empty())
                nic.name = "接口 " + std::to_string(row.InterfaceIndex);
            for (auto *address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next)
            {
                if (address->Address.lpSockaddr == nullptr ||
                    (address->DadState != IpDadStatePreferred && address->DadState != IpDadStateDeprecated))
                    continue;
                std::array<char, INET6_ADDRSTRLEN> text{};
                const auto family = address->Address.lpSockaddr->sa_family;
                if (family == AF_INET && address->Address.iSockaddrLength >= sizeof(sockaddr_in))
                {
                    const auto *ip = reinterpret_cast<const sockaddr_in *>(address->Address.lpSockaddr);
                    if (::InetNtopA(AF_INET, &ip->sin_addr, text.data(), static_cast<DWORD>(text.size())))
                        nic.ipv4.emplace_back(text.data());
                }
                else if (family == AF_INET6 && address->Address.iSockaddrLength >= sizeof(sockaddr_in6))
                {
                    const auto *ip = reinterpret_cast<const sockaddr_in6 *>(address->Address.lpSockaddr);
                    if (ip->sin6_scope_id)
                        continue; // 链路本地地址对面板无意义
                    if (::InetNtopA(AF_INET6, &ip->sin6_addr, text.data(), static_cast<DWORD>(text.size())))
                        nic.ipv6.emplace_back(text.data());
                }
            }
            nic.sampledAt        = sampledAt;
            const bool connected = row.OperStatus == IfOperStatusUp;
            if (connected && row.ReceiveLinkSpeed > 0)
                nic.linkMbps = static_cast<double>(row.ReceiveLinkSpeed) / 1000000.0;
            if (!connected)
            {
                nic.rxBytesPerSec = 0.0;
                nic.txBytesPerSec = 0.0;
            }
            else if (const auto previous = previousCounters_.find(row.InterfaceLuid.Value);
                     previous != previousCounters_.end() && elapsed > 0.0)
            {
                if (row.InOctets >= previous->second.first)
                    nic.rxBytesPerSec = static_cast<double>(row.InOctets - previous->second.first) / elapsed;
                if (row.OutOctets >= previous->second.second)
                    nic.txBytesPerSec = static_cast<double>(row.OutOctets - previous->second.second) / elapsed;
            }
            current[row.InterfaceLuid.Value] = {row.InOctets, row.OutOctets};
            result.push_back(std::move(nic));
        }
        previousCounters_ = std::move(current);
        networkAt_        = now;
        return result;
    }
    [[nodiscard]] auto processes(const std::vector<std::string> &names, std::uint32_t logicalCpus)
        -> std::vector<ProcessInfo> override
    {
        const auto now     = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double>(now - processAt_).count();
        std::map<std::string, double> deltas;
        std::map<std::string, std::uint64_t> memory;
        std::map<std::string, bool> found;
        std::map<std::uint32_t, ProcessCounters> current;

        const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL more = ::Process32FirstW(snapshot, &entry); more != FALSE; more = ::Process32NextW(snapshot, &entry))
            {
                if (entry.th32ProcessID == 0)
                    continue;
                const auto name    = lowercase(toUtf8(entry.szExeFile));
                const auto watched = std::find_if(names.begin(), names.end(),
                                                  [&name](const std::string &want) { return instanceMatches(name, want); });
                if (watched == names.end())
                    continue;
                const HANDLE process =
                    ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
                if (process == nullptr)
                    continue;
                FILETIME creation{}, exit{}, kernel{}, user{};
                ProcessCounters counters;
                if (::GetProcessTimes(process, &creation, &exit, &kernel, &user) != FALSE)
                {
                    ULARGE_INTEGER kernelTime{};
                    ULARGE_INTEGER userTime{};
                    kernelTime.LowPart  = kernel.dwLowDateTime;
                    kernelTime.HighPart = kernel.dwHighDateTime;
                    userTime.LowPart    = user.dwLowDateTime;
                    userTime.HighPart   = user.dwHighDateTime;
                    counters.cpu100ns   = kernelTime.QuadPart + userTime.QuadPart;
                }
                PROCESS_MEMORY_COUNTERS_EX details{};
                details.cb = sizeof(details);
                if (::GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&details), sizeof(details)) !=
                    FALSE)
                    counters.workingSet = details.WorkingSetSize;
                ::CloseHandle(process);

                found[*watched]               = true;
                current[entry.th32ProcessID]  = counters;
                memory[*watched]             += counters.workingSet;
                if (const auto previous = previousProcesses_.find(entry.th32ProcessID);
                    previous != previousProcesses_.end() && elapsed > 0.0 && counters.cpu100ns >= previous->second.cpu100ns)
                    deltas[*watched] += static_cast<double>(counters.cpu100ns - previous->second.cpu100ns);
            }
            ::CloseHandle(snapshot);
        }
        previousProcesses_ = std::move(current);
        processAt_         = now;

        std::vector<ProcessInfo> result;
        for (const auto &name : names)
        {
            ProcessInfo info;
            info.name = name;
            if (!found[name])
            {
                info.state = SZX_METRICS_METRIC_STOPPED;
            }
            else
            {
                // 100ns 计数 → 秒，再除以逻辑核数换算成任务管理器口径的 0~100
                const double seconds = deltas[name] / 10000000.0;
                info.state           = SZX_METRICS_METRIC_OK;
                info.memoryBytes     = memory[name];
                info.cpuPercent =
                    logicalCpus ? std::clamp(100.0 * seconds / elapsed / static_cast<double>(logicalCpus), 0.0, 100.0) : 0.0;
            }
            result.push_back(std::move(info));
        }
        return result;
    }
    [[nodiscard]] auto gpuCount() -> std::uint32_t override
    {
        return nvml_.deviceCount();
    }
    [[nodiscard]] auto gpuName(std::uint32_t index) -> std::string override
    {
        return nvml_.deviceName(index);
    }
    [[nodiscard]] auto gpu(std::uint32_t index) -> MetricValue override
    {
        const auto sample = nvml_.sample(index);
        MetricValue result;
        if (sample.usageValid)
        {
            result.state        = SZX_METRICS_METRIC_OK;
            result.present      = SZX_METRICS_FIELD_USAGE;
            result.usagePercent = sample.usagePercent;
        }
        if (sample.memoryValid)
        {
            result.state = SZX_METRICS_METRIC_OK;
            result.present |=
                SZX_METRICS_FIELD_TOTAL | SZX_METRICS_FIELD_USED | SZX_METRICS_FIELD_AVAILABLE | SZX_METRICS_FIELD_FREE;
            result.totalBytes     = sample.totalBytes;
            result.usedBytes      = sample.totalBytes - sample.freeBytes;
            result.availableBytes = sample.freeBytes;
            result.freeBytes      = sample.freeBytes;
        }
        if (sample.temperatureCelsius)
        {
            result.present            |= SZX_METRICS_FIELD_TEMPERATURE;
            result.temperatureCelsius  = *sample.temperatureCelsius;
        }
        return result;
    }
    [[nodiscard]] auto gpuExtra(std::uint32_t index) -> GpuExtraInfo override
    {
        GpuExtraInfo extra;
        if (!gpuQuery_.open(L"\\GPU Engine(*)\\Utilization Percentage"))
        {
            gpuQuery_.reset();
            return extra;
        }
        if (!gpuQuery_.collect())
        {
            gpuPrimed_ = false;
            return extra;
        }
        if (std::exchange(gpuPrimed_, true))
        {
            // 引擎占用按 engtype 归堆，总占用取各类中的最大值
            std::map<std::string, double> byType;
            const auto instances = collectWildcard(gpuQuery_.counter());
            for (const auto &[instance, value] : instances)
            {
                const std::string lower = lowercase(instance);
                const auto typeAt       = lower.rfind("engtype_");
                if (typeAt == std::string::npos)
                    continue;
                byType[lower.substr(typeAt + 8)] += value;
            }
            if (!instances.empty())
            {
                extra.state = SZX_METRICS_METRIC_OK;
                for (const auto &[type, sum] : byType)
                {
                    const double clamped = std::clamp(sum, 0.0, 100.0);
                    if (type == "cuda")
                        extra.cudaPercent = clamped;
                    else if (type == "3d")
                        extra.graphics3dPercent = clamped;
                    extra.totalPercent = std::max(extra.totalPercent.value_or(0.0), clamped);
                }
            }
        }
        // 按进程显存：WDDM 下 NVML 恒报不可用，优先用 GPU Process Memory 计数器
        std::map<std::string, std::uint64_t> totals;
        bool available = false;
        if (gpuProcessMemory_.open(L"\\GPU Process Memory(*)\\Dedicated Usage") && gpuProcessMemory_.collect())
        {
            const auto instances = collectWildcard(gpuProcessMemory_.counter());
            available            = !instances.empty();
            for (const auto &[instance, value] : instances)
            {
                unsigned pid = 0;
                if (!(value > 0.0) || !pidFromInstance(instance, pid))
                    continue;
                const auto name = processNameOfPid(pid);
                if (!name.empty())
                    totals[name] += static_cast<std::uint64_t>(value);
            }
        }
        if (totals.empty())
        {
            const auto memory = nvml_.processMemory(index);
            available         = available || !memory.empty();
            for (const auto &[pid, bytes] : memory)
            {
                const auto name = processNameOfPid(pid);
                if (!name.empty())
                    totals[name] += bytes;
            }
        }
        if (available)
        {
            extra.state         = SZX_METRICS_METRIC_OK;
            extra.vramByProcess = std::move(totals);
        }
        return extra;
    }

private:
    PdhQuery cpuQuery_;
    bool primed_{};
    PdhQuery gpuQuery_;
    bool gpuPrimed_{};
    PdhQuery gpuProcessMemory_;
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> previousCounters_;
    std::chrono::steady_clock::time_point networkAt_{};
    std::map<std::uint32_t, ProcessCounters> previousProcesses_;
    std::chrono::steady_clock::time_point processAt_{};
    std::int64_t nextTemperatureAt_{};
    std::optional<double> lastTemperature_;
    NvmlLoader nvml_;
};
} // namespace

auto createCollector() -> std::unique_ptr<Collector>
{
    return std::make_unique<WindowsCollector>();
}
} // namespace v1
} // namespace SZX::METRICS::DETAIL
