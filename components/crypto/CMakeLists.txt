add_library(szx_crypto SHARED
    src/Base64.cc
    src/Fernet.cc
    src/Hash.cc
    src/Rsa.cc
    src/Random.cc
    src/SecureBytes.cc
    include/SzxCrypto/Base64.h
    include/SzxCrypto/Crypto.h
    include/SzxCrypto/CryptoExport.h
    include/SzxCrypto/Fernet.h
    include/SzxCrypto/Hash.h
    include/SzxCrypto/Rsa.h
    include/SzxCrypto/Random.h
    include/SzxCrypto/SecureBytes.h
)

add_library(szx::crypto ALIAS szx_crypto)

set_target_properties(szx_crypto PROPERTIES EXPORT_NAME crypto)
target_include_directories(szx_crypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/common>
)

target_link_libraries(szx_crypto PUBLIC szx::common)
target_compile_definitions(szx_crypto PRIVATE SZX_CRYPTO_EXPORTS)
target_compile_features(szx_crypto PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_crypto)

if(WIN32)
    target_link_libraries(szx_crypto PRIVATE Bcrypt)
endif()
