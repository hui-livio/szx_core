/**
 * @file CryptoExport.h
 * @brief szx_crypto 动态库导出宏。
 *
 * 为共享库构建定义公共符号导入和导出所需的 SZX_CRYPTO_API。
 */

#pragma once

#if defined(SZX_CRYPTO_USE_STATIC) || defined(SZX_CRYPTO_USE_SOURCE)
    #define SZX_CRYPTO_API
#else
    #if defined(_WIN32) || defined(WIN64)
        #if defined(SZX_CRYPTO_EXPORTS)
            #define SZX_CRYPTO_API __declspec(dllexport)
        #else
            #define SZX_CRYPTO_API __declspec(dllimport)
        #endif
    #else
        #define SZX_CRYPTO_API __attribute__((visibility("default")))
    #endif
#endif
