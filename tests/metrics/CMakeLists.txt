szx_add_test(NAME test_szx_metrics
    SOURCES Metrics_test.cc
    LIBS szx::metrics)
if(TARGET test_szx_metrics)
    if(WIN32)
        # DLL 内容变化时导入库可能不变，仍需重新触发运行库复制。
        set_property(TARGET test_szx_metrics APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:szx_metrics>")
    endif()
    szx_copy_runtime_dlls(test_szx_metrics)
endif()
# 同一份用例再链接静态库，验证 SZX_METRICS_STATIC 分支头文件与符号解析。
szx_add_test(NAME test_szx_metrics_static
    SOURCES Metrics_test.cc
    LIBS szx::metrics_static)
