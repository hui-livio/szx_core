/**
 * @file UIToolkitsExport.h
 * @brief UIToolkits 动态库导出宏定义。
 */

#pragma once

#if defined(UITOOLKITS_STATIC)
    #define UITOOLKITS_API
#elif defined(_WIN32) || defined(WIN64)
    #if defined(UITOOLKITS_EXPORTS)
        #define UITOOLKITS_API __declspec(dllexport)
    #else
        #define UITOOLKITS_API __declspec(dllimport)
    #endif
#else
    #define UITOOLKITS_API __attribute__((visibility("default")))
#endif
