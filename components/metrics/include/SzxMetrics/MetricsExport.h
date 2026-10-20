#pragma once
/** @file MetricsExport.h
 * @brief 指标采集组件的符号可见性宏。
 * @details 同一份公共头文件同时服务于三种使用方式：
 *          1. 动态库（默认）：Windows 导出/导入 __declspec，Linux 走 default 可见性；
 *          2. 静态库：链接 szx_metrics_static 时由 CMake 传入 SZX_METRICS_STATIC，
 *             宏展开为空，符号按静态链接规则解析；
 *          3. 源码编译：调用方直接把 src/ 编进自己的目标时定义
 *             SZX_METRICS_USE_SOURCE，行为与静态库一致。
 *          因此不要在公共头里出现任何依赖 DLL 的写法（如显式 LoadLibrary 约定）。
 */

#if defined(SZX_METRICS_STATIC) || defined(SZX_METRICS_USE_SOURCE)
    #define SZX_METRICS_API
#elif defined(_WIN32) || defined(WIN64)
    #if defined(SZX_METRICS_EXPORTS)
        #define SZX_METRICS_API __declspec(dllexport)
    #else
        #define SZX_METRICS_API __declspec(dllimport)
    #endif
#else
    #define SZX_METRICS_API __attribute__((visibility("default")))
#endif
