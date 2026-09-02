add_library(szx_keystore SHARED
    src/KeyStore.cc include/SzxKeyStore/KeyStore.h
)

add_library(szx::keystore ALIAS szx_keystore)
set_target_properties(szx_keystore PROPERTIES EXPORT_NAME keystore)
target_include_directories(szx_keystore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(szx_keystore PUBLIC szx::common szx::crypto)
target_compile_definitions(szx_keystore PRIVATE SZX_KEYSTORE_EXPORTS)
target_compile_features(szx_keystore PUBLIC cxx_std_20)
if(WIN32)
 target_link_libraries(szx_keystore PRIVATE ncrypt Bcrypt)
endif()
