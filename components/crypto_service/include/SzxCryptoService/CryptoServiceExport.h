#pragma once
#if defined(SZX_CRYPTO_SERVICE_STATIC)
    #define SZX_CRYPTO_SERVICE_API
#elif defined(_WIN32)
    #if defined(SZX_CRYPTO_SERVICE_EXPORTS)
        #define SZX_CRYPTO_SERVICE_API __declspec(dllexport)
    #else
        #define SZX_CRYPTO_SERVICE_API __declspec(dllimport)
    #endif
    #define SZX_CRYPTO_API SZX_CRYPTO_SERVICE_API
#else
    #define SZX_CRYPTO_SERVICE_API __attribute__((visibility("default")))
#endif
