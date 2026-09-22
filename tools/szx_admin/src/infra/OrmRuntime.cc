#include "OrmRuntime.h"

#include <iostream>
#include <streambuf>

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
thread_local bool suppressDiagnostics{};

// Install once before main, never swap cout's buffer while worker threads run.
// Only the thread currently inside an ORM call is muted. Other application
// output keeps its normal destination, including concurrent stdout writers.
class DiagnosticBuffer final : public std::streambuf
{
public:
    DiagnosticBuffer() :
        destination_(std::cout.rdbuf(this))
    {
    }
    ~DiagnosticBuffer() override
    {
        std::cout.rdbuf(destination_);
    }

protected:
    auto xsputn(const char *data, std::streamsize size) -> std::streamsize override
    {
        if (suppressDiagnostics)
            return size;
        std::lock_guard lock(mutex_);
        return destination_->sputn(data, size);
    }

    auto overflow(int_type value) -> int_type override
    {
        if (traits_type::eq_int_type(value, traits_type::eof()))
            return traits_type::not_eof(value);
        if (suppressDiagnostics)
            return value;
        std::lock_guard lock(mutex_);
        return destination_->sputc(traits_type::to_char_type(value));
    }

    auto sync() -> int override
    {
        if (suppressDiagnostics)
            return 0;
        std::lock_guard lock(mutex_);
        return destination_->pubsync();
    }

private:
    std::streambuf *destination_;
    std::mutex mutex_;
};

std::ios_base::Init initializeStreams;
DiagnosticBuffer diagnosticBuffer;
} // namespace

auto ormMutex() -> std::recursive_timed_mutex &
{
    static std::recursive_timed_mutex mutex;
    return mutex;
}

OrmDiagnosticsScope::OrmDiagnosticsScope() noexcept :
    previous_(suppressDiagnostics)
{
    suppressDiagnostics = true;
}

OrmDiagnosticsScope::~OrmDiagnosticsScope()
{
    suppressDiagnostics = previous_;
}
} // namespace SZX::ADMIN::INFRA::DETAIL
