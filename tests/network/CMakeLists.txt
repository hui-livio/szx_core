szx_add_test(NAME test_modules_network SOURCES HttpServer_test.cc HttpClient_test.cc LIBS szx::network LABELS network)
if(TARGET test_modules_network)
    if(WIN32)
        target_link_libraries(test_modules_network PRIVATE ws2_32)
    endif()
    target_compile_definitions(test_modules_network PRIVATE
        SZX_NETWORK_TEST_TLS=$<BOOL:${SZX_NETWORK_ENABLE_TLS}>
        SZX_NETWORK_TEST_HTTP_CLIENT=$<BOOL:${SZX_NETWORK_ENABLE_HTTP_CLIENT}>)
    if(SZX_CTEST_REGISTRATION STREQUAL "binary")
        set_tests_properties(test_modules_network PROPERTIES TIMEOUT 60)
    endif()
endif()
