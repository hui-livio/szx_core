#pragma once

/**
 * @file DmcExport.hpp
 * @brief DMC 动态库导出和导入宏定义。
 *
 * 静态链接时导出宏为空；Windows 动态库和支持可见性属性的平台使用对应声明。
 */

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
    #if defined(SZX_DMC_STATIC) || defined(SZX_DMC_SOURCE)
        #define SZX_DMC_API
    #elif defined(SZX_DMC_EXPORTS) || defined(SZX_DMC_EXPORT)
        #define SZX_DMC_API __declspec(dllexport)
    #else
        #define SZX_DMC_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define SZX_DMC_API __attribute__((visibility("default")))
#else
    #define SZX_DMC_API
#endif
