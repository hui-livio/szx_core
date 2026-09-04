add_library(szx_license SHARED
    src/License.cc
    src/LicenseJson.hpp
    include/SzxLicense/License.h
    include/SzxLicense/Issuer.h
    include/SzxLicense/LicenseExport.h
)
add_library(szx::license ALIAS szx_license)
set_target_properties(szx_license PROPERTIES EXPORT_NAME license)
target_include_directories(szx_license PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../crypto_service/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/ext>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(szx_license PUBLIC szx::crypto_service)
target_compile_features(szx_license PUBLIC cxx_std_20)
target_compile_definitions(szx_license PRIVATE SZX_LICENSE_EXPORTS)
szx_apply_default_compile_policy(szx_license)
