#pragma once
/** @file NetworkExport.h
 * @brief Network DLL visibility. Consumers must use the same C++ runtime/ABI.
 */
#if defined(_WIN32)
    #if defined(SZX_NETWORK_EXPORTS)
        #define SZX_NETWORK_API __declspec(dllexport)
    #else
        #define SZX_NETWORK_API __declspec(dllimport)
    #endif
#else
    #define SZX_NETWORK_API __attribute__((visibility("default")))
#endif
