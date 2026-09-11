szx_add_test(NAME test_rbac_auth SOURCES JwtService_test.cc LIBS szx::rbac_auth szx::crypto)
szx_add_test(NAME test_rbac_infra SOURCES Infrastructure_test.cc LIBS szx::rbac_infra)
szx_add_test(NAME test_rbac_database SOURCES Database_test.cc LIBS szx::rbac_infra)
