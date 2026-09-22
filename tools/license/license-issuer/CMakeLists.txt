add_executable(szx_license_issuer main.cc)
target_link_libraries(szx_license_issuer PRIVATE szx::license szx::crypto szx::common)
target_compile_features(szx_license_issuer PRIVATE cxx_std_20)
szx_apply_default_compile_policy(szx_license_issuer)
