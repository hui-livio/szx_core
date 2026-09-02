#pragma once
#if defined(SZX_KEYSTORE_STATIC)
    #define SZX_KEYSTORE_API
#elif defined(_WIN32)
    #if defined(SZX_KEYSTORE_EXPORTS)
        #define SZX_KEYSTORE_API __declspec(dllexport)
    #else
        #define SZX_KEYSTORE_API __declspec(dllimport)
    #endif
    #define SZX_CRYPTO_API SZX_KEYSTORE_API
#else
    #define SZX_KEYSTORE_API __attribute__((visibility("default")))
#endif
