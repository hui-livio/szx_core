#pragma once

#if defined(_WIN32) && defined(SZX_LICENSE_EXPORTS)
    #define SZX_LICENSE_API __declspec(dllexport)
#elif defined(_WIN32)
    #define SZX_LICENSE_API __declspec(dllimport)
#else
    #define SZX_LICENSE_API
#endif
