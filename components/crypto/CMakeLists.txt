add_library(crypto SHARED
    src/Base64.cc src/Fernet.cc src/Hash.cc
    include/SzxCrypto/Base64.h include/SzxCrypto/Crypto.h
    include/SzxCrypto/CryptoExport.h include/SzxCrypto/Fernet.h include/SzxCrypto/Hash.h)
add_library(szx::crypto ALIAS crypto)
target_include_directories(crypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/common>)
target_link_libraries(crypto PUBLIC szx::common)
target_compile_definitions(crypto PRIVATE SZX_CRYPTO_EXPORTS)
target_compile_features(crypto PUBLIC cxx_std_20)
szx_apply_default_compile_policy(crypto)
if (WIN32)
    target_link_libraries(crypto PRIVATE Bcrypt)
endif()
