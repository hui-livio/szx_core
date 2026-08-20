/**
 * @file CryptoExport.h
 * @brief szx_crypto dynamic library export macros.
 *
 * Defines SZX_CRYPTO_API for importing and exporting public szx_crypto symbols
 * from shared-library builds.
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
