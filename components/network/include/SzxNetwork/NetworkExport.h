#pragma once
/** @file NetworkExport.h
 * @brief 网络模块 DLL 符号可见性；调用方必须使用匹配的 C++ 运行时和 ABI。
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
