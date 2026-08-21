/**
 * @file LocalFileFeatureProvider.cc
 * @brief 本地 JSON/YAML Feature 文件读写实现。
 */

#include "LocalFileFeatureProvider.h"
#include "FeatureValidation.h"

#include "Serializer.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

using namespace SZX::COMMON;

namespace SZX::FEATURE
{
namespace
{
constexpr int kFileError                   = 1;
constexpr int kParseError                  = 2;
constexpr std::uintmax_t kMaximumFileBytes = 4U * 1024U * 1024U;

struct FeatureRecord
{
    std::string name;
    std::string type;
    std::string value;
};

struct FeatureDocument
{
    std::int64_t version = 0;
    std::string encoding;
    std::vector<FeatureRecord> features;
};

struct FeatureHeader
{
    std::int64_t version = 0;
    std::string encoding;
};

SZX_REFL(FeatureRecord, name, type, value)
SZX_REFL(FeatureDocument, version, encoding, features)
SZX_REFL(FeatureHeader, version, encoding)

std::string preciseDouble(double value)
{
    char buffer[128]{};
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value, std::chars_format::general,
                                      std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{})
        throw std::runtime_error("failed to serialize floating-point feature value");
    return {buffer, result.ptr};
}

FeatureRecord toRecord(const std::string &name, const FeatureValue &value)
{
    FeatureRecord record;
    record.name = name;
    std::visit(
        [&record](const auto &item)
        {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                record.type  = "null";
                record.value = "__null__";
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                record.type  = "bool";
                record.value = item ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, std::int64_t>)
            {
                record.type  = "integer";
                record.value = std::to_string(item);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                record.type  = "number";
                record.value = preciseDouble(item);
            }
            else
            {
                record.type  = "string";
                record.value = item;
            }
        },
        value);
    return record;
}

std::string hexEncode(std::string_view value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string encoded            = "hex_";
    encoded.reserve(4 + value.size() * 2);
    for (const unsigned char byte : value)
    {
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 0x0f]);
    }
    return encoded;
}

Result<std::string> hexDecode(std::string_view value)
{
    using StringResult = Result<std::string>;
    if (!value.starts_with("hex_"))
        return StringResult::Success(std::string{value});
    value.remove_prefix(4);
    if (value.size() % 2 != 0)
        return StringResult::Failure(kParseError, "invalid hex-encoded YAML feature field");

    auto nibble = [](char digit) -> int
    {
        if (digit >= '0' && digit <= '9')
            return digit - '0';
        if (digit >= 'a' && digit <= 'f')
            return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F')
            return digit - 'A' + 10;
        return -1;
    };

    std::string decoded;
    decoded.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2)
    {
        const int high = nibble(value[index]);
        const int low  = nibble(value[index + 1]);
        if (high < 0 || low < 0)
            return StringResult::Failure(kParseError, "invalid hex-encoded YAML feature field");
        decoded.push_back(static_cast<char>((high << 4) | low));
    }
    return StringResult::Success(std::move(decoded));
}

void encodeYamlRecord(FeatureRecord &record)
{
    const auto encodeIfNeeded = [](std::string &field, bool protectYamlScalars)
    {
        std::string lower;
        lower.resize(field.size());
        std::transform(field.begin(), field.end(), lower.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        const bool reservedScalar = lower == "~" || lower == "null" || lower == "true" || lower == "false" || lower == "yes" ||
            lower == "no" || lower == "on" || lower == "off" || lower == ".nan" || lower == ".inf" || lower == "-.inf";
        const bool numericScalar = !field.empty() &&
            (std::isdigit(static_cast<unsigned char>(field.front())) ||
             ((field.front() == '+' || field.front() == '-') && field.size() > 1 &&
              std::isdigit(static_cast<unsigned char>(field[1]))));
        const bool safe = !field.empty() && !field.starts_with("hex_") &&
            (!protectYamlScalars || (!reservedScalar && !numericScalar)) &&
            std::all_of(
                field.begin(), field.end(), [](unsigned char value)
                { return std::isalnum(value) || value == '_' || value == '.' || value == '-' || value == '/' || value == '+'; });
        if (!safe)
            field = hexEncode(field);
    };
    const bool stringValue = record.type == "string";
    encodeIfNeeded(record.name, true);
    encodeIfNeeded(record.type, true);
    encodeIfNeeded(record.value, stringValue);
}

Result<FeatureRecord> decodeYamlRecord(FeatureRecord record)
{
    using RecordResult = Result<FeatureRecord>;
    auto name          = hexDecode(record.name);
    auto type          = hexDecode(record.type);
    auto value         = hexDecode(record.value);
    if (!name.ok)
        return RecordResult::Failure(name.error, name.message);
    if (!type.ok)
        return RecordResult::Failure(type.error, type.message);
    if (!value.ok)
        return RecordResult::Failure(value.error, value.message);
    record.name  = std::move(name.value);
    record.type  = std::move(type.value);
    record.value = std::move(value.value);
    return RecordResult::Success(std::move(record));
}

Result<FeatureValue> fromRecord(const FeatureRecord &record)
{
    using ValueResult = Result<FeatureValue>;
    if (record.name.empty())
        return ValueResult::Failure(kParseError,
                                    "feature name must not be empty (type='" + record.type + "', value='" + record.value + "')");
    if (record.type == "null")
    {
        if (record.value != "__null__")
            return ValueResult::Failure(kParseError, "invalid null feature value");
        return ValueResult::Success({});
    }
    if (record.type == "bool")
    {
        if (record.value == "true")
            return ValueResult::Success(true);
        if (record.value == "false")
            return ValueResult::Success(false);
        return ValueResult::Failure(kParseError, "invalid boolean feature value: " + record.value);
    }
    if (record.type == "integer")
    {
        std::int64_t value = 0;
        const char *end    = record.value.data() + record.value.size();
        const auto result  = std::from_chars(record.value.data(), end, value);
        if (result.ec != std::errc{} || result.ptr != end)
            return ValueResult::Failure(kParseError, "invalid integer feature value: " + record.value);
        return ValueResult::Success(value);
    }
    if (record.type == "number")
    {
        double value      = 0.0;
        const char *end   = record.value.data() + record.value.size();
        const auto result = std::from_chars(record.value.data(), end, value, std::chars_format::general);
        if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value))
            return ValueResult::Failure(kParseError, "invalid numeric feature value: " + record.value);
        return ValueResult::Success(value);
    }
    if (record.type == "string")
        return ValueResult::Success(record.value);
    return ValueResult::Failure(kParseError, "unknown feature type: " + record.type);
}

std::filesystem::path temporaryPathFor(const std::filesystem::path &target)
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto tick  = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary   = target;
    temporary       += ".tmp." + std::to_string(tick) + "." + std::to_string(sequence.fetch_add(1));
    return temporary;
}

bool replaceAtomically(const std::filesystem::path &source, const std::filesystem::path &target)
{
#ifdef _WIN32
    return MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    return std::rename(source.c_str(), target.c_str()) == 0;
#endif
}

bool flushFileToDisk(const std::filesystem::path &path)
{
#ifdef _WIN32
    HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    const bool flushed = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    return flushed;
#else
    const int file = ::open(path.c_str(), O_RDONLY);
    if (file < 0)
        return false;
    const bool flushed = ::fsync(file) == 0;
    ::close(file);
    return flushed;
#endif
}

bool flushParentDirectory(const std::filesystem::path &path)
{
#ifdef _WIN32
    return true;
#else
    const auto parent   = path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path();
    const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0)
        return false;
    const bool flushed = ::fsync(directory) == 0;
    ::close(directory);
    return flushed;
#endif
}
} // namespace

LocalFileFeatureProvider::LocalFileFeatureProvider(std::filesystem::path filePath, Format format) :
    m_filePath(std::move(filePath)),
    m_format(format)
{
}

FeatureLoadResult LocalFileFeatureProvider::load()
{
    std::ifstream input(m_filePath, std::ios::binary);
    if (!input)
        return FeatureLoadResult::Failure(kFileError, "unable to open feature file: " + m_filePath.string());
    input.seekg(0, std::ios::end);
    const auto inputSize = input.tellg();
    if (inputSize < 0 || static_cast<std::uintmax_t>(inputSize) > kMaximumFileBytes)
        return FeatureLoadResult::Failure(kFileError, "feature file exceeds size limit");
    input.seekg(0, std::ios::beg);

    const std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    FeatureDocument document;
    std::error_code error;
    const auto format = resolvedFormat();
    if (format == Format::Yaml)
    {
        const auto features      = content.find("features:");
        const bool emptySequence = features != std::string::npos && content.find('-', features) == std::string::npos;
        if (emptySequence)
        {
            FeatureHeader header;
            Serializer::from_yaml(header, content, error);
            document.version  = header.version;
            document.encoding = std::move(header.encoding);
        }
        else
        {
            Serializer::from_yaml(document, content, error);
        }
    }
    else
        Serializer::from_json(document, content, error);
    if (error)
        return FeatureLoadResult::Failure(kParseError, "unable to parse feature file: " + error.message());
    if (document.version != 1)
        return FeatureLoadResult::Failure(kParseError, "unsupported feature document version");
    if (format == Format::Yaml)
    {
        if (document.encoding.empty())
            document.encoding = "plain";
        if (document.encoding != "plain" && document.encoding != "mixed-v1" && document.encoding != "hex-v1")
            return FeatureLoadResult::Failure(kParseError, "unsupported YAML feature encoding");
    }
    else if (!document.encoding.empty() && document.encoding != "plain")
    {
        return FeatureLoadResult::Failure(kParseError, "unsupported JSON feature encoding");
    }
    if (document.features.size() > DETAIL::kMaximumFeatures)
        return FeatureLoadResult::Failure(kParseError, "feature count exceeds limit");

    FeatureMap result;
    for (std::size_t recordIndex = 0; recordIndex < document.features.size(); ++recordIndex)
    {
        auto record = document.features[recordIndex];
        // iguana's YAML sequence reader materializes the writer's leading
        // standalone "-" marker as an empty first record. Depending on the
        // following scalar types its value field can inherit parser scratch
        // state, so identify the sentinel by position/name/type.
        if (format == Format::Yaml && recordIndex == 0 && record.name.empty() && record.type.empty())
            continue;
        if (format == Format::Yaml && (document.encoding == "mixed-v1" || document.encoding == "hex-v1"))
        {
            auto decoded = decodeYamlRecord(std::move(record));
            if (!decoded.ok)
                return FeatureLoadResult::Failure(decoded.error, decoded.message);
            record = std::move(decoded.value);
        }
        if (record.name.size() > DETAIL::kMaximumNameBytes || record.value.size() > DETAIL::kMaximumValueBytes)
            return FeatureLoadResult::Failure(kParseError, "feature field exceeds size limit");
        if (result.contains(record.name))
            return FeatureLoadResult::Failure(kParseError, "duplicate feature name: " + record.name);
        auto parsed = fromRecord(record);
        if (!parsed.ok)
            return FeatureLoadResult::Failure(parsed.error, parsed.message);
        result.emplace(record.name, std::move(parsed.value));
    }
    {
        std::scoped_lock lock(m_statusMutex);
        m_status = {};
    }
    return FeatureLoadResult::Success(std::move(result));
}

FeaturePersistResult LocalFileFeatureProvider::save(const FeatureMap &features)
{
    const auto validation = DETAIL::validateFeatures(features);
    if (!validation.ok)
        return FeaturePersistResult::Failure(validation.error, validation.message);
    try
    {
        FeatureDocument document;
        document.version = 1;
        document.features.reserve(features.size());
        for (const auto &[name, value] : features)
            document.features.push_back(toRecord(name, value));
        std::sort(document.features.begin(), document.features.end(),
                  [](const FeatureRecord &left, const FeatureRecord &right) { return left.name < right.name; });

        const auto format = resolvedFormat();
        if (format == Format::Yaml)
        {
            document.encoding = "mixed-v1";
            for (auto record = document.features.begin(); record != document.features.end(); ++record)
                encodeYamlRecord(*record);
        }
        else
        {
            document.encoding = "plain";
        }

        std::string content;
        if (format == Format::Yaml)
            Serializer::to_yaml(document, content);
        else
            Serializer::to_json(document, content);
        if (content.size() > kMaximumFileBytes)
            return FeaturePersistResult::Failure(kFileError, "serialized feature file exceeds size limit");

        std::error_code error;
        const auto parent = m_filePath.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, error);
            if (error)
                return FeaturePersistResult::Failure(kFileError, "unable to create feature directory: " + error.message());
        }

        const auto temporaryPath = temporaryPathFor(m_filePath);
        {
            std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!output)
                return FeaturePersistResult::Failure(kFileError, "unable to create temporary feature file");
            output.write(content.data(), static_cast<std::streamsize>(content.size()));
            output.flush();
            output.close();
            if (!output)
            {
                std::filesystem::remove(temporaryPath, error);
                return FeaturePersistResult::Failure(kFileError, "unable to write temporary feature file");
            }
        }

        if (!flushFileToDisk(temporaryPath))
        {
            std::filesystem::remove(temporaryPath, error);
            return FeaturePersistResult::Failure(kFileError, "unable to flush temporary feature file");
        }
        if (!replaceAtomically(temporaryPath, m_filePath))
        {
            std::filesystem::remove(temporaryPath, error);
            return FeaturePersistResult::Failure(kFileError, "unable to atomically replace feature file");
        }
        // The rename is already visible at this point. A directory fsync
        // failure can only weaken crash durability; it must not be reported as
        // an uncommitted write because callers would diverge from disk state.
        const bool directoryDurable = flushParentDirectory(m_filePath);
        {
            std::scoped_lock lock(m_statusMutex);
            m_status = directoryDurable ?
                FeatureProviderStatus{} :
                FeatureProviderStatus{true, {"feature file committed, but directory durability could not be confirmed"}};
        }
        return FeaturePersistResult::Success(features);
    } catch (const std::exception &error)
    {
        return FeaturePersistResult::Failure(kFileError, error.what());
    } catch (...)
    {
        return FeaturePersistResult::Failure(kFileError, "feature serialization failed with an unknown exception");
    }
}

FeatureProviderStatus LocalFileFeatureProvider::status() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

LocalFileFeatureProvider::Format LocalFileFeatureProvider::resolvedFormat() const
{
    if (m_format != Format::Auto)
        return m_format;
    auto extension = m_filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".yaml" || extension == ".yml" ? Format::Yaml : Format::Json;
}

std::unique_ptr<IFeatureProvider> makeLocalFileFeatureProvider(std::filesystem::path filePath, FeatureFileFormat format)
{
    LocalFileFeatureProvider::Format internalFormat = LocalFileFeatureProvider::Format::Auto;
    if (format == FeatureFileFormat::Json)
        internalFormat = LocalFileFeatureProvider::Format::Json;
    else if (format == FeatureFileFormat::Yaml)
        internalFormat = LocalFileFeatureProvider::Format::Yaml;
    return std::make_unique<LocalFileFeatureProvider>(std::move(filePath), internalFormat);
}
} // namespace SZX::FEATURE
