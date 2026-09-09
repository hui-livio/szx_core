#pragma once

/**
 * @file LicenseExport.h
 * @brief SZX License 动态库导出和导入宏定义。
 */

#if defined(_WIN32) && defined(SZX_LICENSE_EXPORTS)
    /** @brief 构建 SZX License 动态库时导出公共符号。 */
    #define SZX_LICENSE_API __declspec(dllexport)
#elif defined(_WIN32)
    /** @brief 使用 SZX License 动态库时导入公共符号。 */
    #define SZX_LICENSE_API __declspec(dllimport)
#else
    /** @brief 非 Windows 平台无需额外的符号修饰。 */
    #define SZX_LICENSE_API
#endif
