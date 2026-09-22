add_executable(szx_license_user_demo main.cc)

target_link_libraries(szx_license_user_demo
    PRIVATE
        szx::license
        szx::crypto
)

target_compile_features(szx_license_user_demo PRIVATE cxx_std_20)
szx_apply_default_compile_policy(szx_license_user_demo)
