add_library(szx_rbac_auth STATIC
    src/auth/JwtService.cc
)
add_library(szx::rbac_auth ALIAS szx_rbac_auth)

target_include_directories(szx_rbac_auth
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/jwt-cpp/include
        ${SZX_OPENSSL_ROOT}/include
)

target_link_libraries(szx_rbac_auth
    PUBLIC szx::crypto_service
)

target_compile_features(szx_rbac_auth PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_rbac_auth)
