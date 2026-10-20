# 指标采集组件：对象库承载公共源码，共享库与静态库各自从同一批对象产出，
# 保证两种形态行为完全一致，也避免平台源码被编译两次。
add_library(szx_metrics_objects OBJECT
    src/MetricsParsers.cc
    src/MetricsService.cc
    src/Platforms/NvmlLoader.cc
    src/MetricsInternal.h
    src/MetricsAbi.h
    src/Platforms/NvmlLoader.h
    include/SzxMetrics/Metrics.h
    include/SzxMetrics/MetricsTypes.h
    include/SzxMetrics/MetricsExport.h
)
set_target_properties(szx_metrics_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(szx_metrics_objects PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(szx_metrics_objects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(szx_metrics_objects PRIVATE szx::common)
target_compile_definitions(szx_metrics_objects PRIVATE SZX_METRICS_EXPORTS)
target_compile_features(szx_metrics_objects PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_metrics_objects)

# 平台采集器各自一份实现，公共层只依赖 DETAIL::Collector 接口。两种库都要链接，
# 所以写成对象库，被两个目标共同引用。
add_library(szx_metrics_platform_objects OBJECT)
set_target_properties(szx_metrics_platform_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(szx_metrics_platform_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(szx_metrics_platform_objects PRIVATE szx::common)
target_compile_features(szx_metrics_platform_objects PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_metrics_platform_objects)
if(WIN32)
    target_sources(szx_metrics_platform_objects PRIVATE src/Platforms/PlatformWindows.cc)
else()
    target_sources(szx_metrics_platform_objects PRIVATE src/Platforms/PlatformLinux.cc)
endif()

add_library(szx_metrics SHARED
    $<TARGET_OBJECTS:szx_metrics_objects>
    $<TARGET_OBJECTS:szx_metrics_platform_objects>)
add_library(szx_metrics_static STATIC
    $<TARGET_OBJECTS:szx_metrics_objects>
    $<TARGET_OBJECTS:szx_metrics_platform_objects>)
add_library(szx::metrics ALIAS szx_metrics)
add_library(szx::metrics_static ALIAS szx_metrics_static)

set_target_properties(szx_metrics PROPERTIES
    EXPORT_NAME metrics
    VERSION 1.0.0
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
set_target_properties(szx_metrics_static PROPERTIES EXPORT_NAME metrics_static)

# 公共头目录：两种形态相同，静态库额外向消费者传播 SZX_METRICS_STATIC，
# 让 MetricsExport.h 展开为空宏而不是 dllimport。
foreach(_metrics_target IN ITEMS szx_metrics szx_metrics_static)
    target_include_directories(${_metrics_target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(${_metrics_target} PUBLIC szx::common)
    target_compile_features(${_metrics_target} PUBLIC cxx_std_20)
    szx_apply_default_compile_policy(${_metrics_target})
endforeach()
target_compile_definitions(szx_metrics_static INTERFACE SZX_METRICS_STATIC)

# 系统库：Windows 的性能计数器 / 网络接口 / 进程信息，Linux 的 dlopen。
if(WIN32)
    foreach(_metrics_target IN ITEMS szx_metrics szx_metrics_static)
        target_link_libraries(${_metrics_target} PRIVATE Pdh Iphlpapi Psapi Ws2_32 Wbemuuid)
    endforeach()
else()
    foreach(_metrics_target IN ITEMS szx_metrics szx_metrics_static)
        target_link_libraries(${_metrics_target} PRIVATE ${CMAKE_DL_LIBS})
    endforeach()
endif()

if(SZX_COMPONENTS_INSTALL)
    szx_install_target(szx_metrics)
    szx_install_target(szx_metrics_static)
    szx_install_headers(metrics HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" DESTINATION . FILE_PATTERN "*.h")
endif()
