#include <hv/hlog.h>

namespace
{
struct LibhvLogging final
{
    LibhvLogging()
    {
        hlog_disable();
    }
};

const LibhvLogging libhvLogging;
} // namespace
