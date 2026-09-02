#pragma once
#if defined(SZX_CRYPTO_FILE_STATIC)
#define SZX_CRYPTO_FILE_API
#elif defined(_WIN32)
#if defined(SZX_CRYPTO_FILE_EXPORTS)
#define SZX_CRYPTO_FILE_API __declspec(dllexport)
#else
#define SZX_CRYPTO_FILE_API __declspec(dllimport)
#endif
#else
#define SZX_CRYPTO_FILE_API __attribute__((visibility("default")))
#endif
