add_library(szx_crypto_service SHARED
    src/CryptoService.cc
    include/SzxCryptoService/CryptoService.h
)
add_library(szx::crypto_service ALIAS szx_crypto_service)
target_include_directories(szx_crypto_service PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>)
target_link_libraries(szx_crypto_service PUBLIC szx::crypto)
target_compile_features(szx_crypto_service PUBLIC cxx_std_20)
if(WIN32)
    target_compile_definitions(szx_crypto_service PRIVATE SZX_CRYPTO_EXPORTS)
endif()
