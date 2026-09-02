#pragma once

#if defined(SZX_COMPRESSION_USE_STATIC) || defined(SZX_COMPRESSION_USE_SOURCE)
    #define SZX_COMPRESSION_API
#elif defined(_WIN32) || defined(WIN64)
    #if defined(SZX_COMPRESSION_EXPORTS)
        #define SZX_COMPRESSION_API __declspec(dllexport)
    #else
        #define SZX_COMPRESSION_API __declspec(dllimport)
    #endif
#else
    #define SZX_COMPRESSION_API __attribute__((visibility("default")))
#endif
