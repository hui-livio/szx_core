add_library(szx_crypto_file SHARED src/BufferFile.cc include/SzxCryptoFile/BufferFile.h)
add_library(szx::crypto_file ALIAS szx_crypto_file)
set_target_properties(szx_crypto_file PROPERTIES EXPORT_NAME crypto_file)
target_include_directories(szx_crypto_file PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(szx_crypto_file PUBLIC szx::crypto_service)
target_compile_definitions(szx_crypto_file PRIVATE SZX_CRYPTO_FILE_EXPORTS)
target_compile_features(szx_crypto_file PUBLIC cxx_std_20)
