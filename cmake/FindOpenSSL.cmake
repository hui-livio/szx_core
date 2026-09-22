include("${CMAKE_CURRENT_LIST_DIR}/szx_find_openssl.cmake")

set(_szx_openssl_args)
if(OpenSSL_FIND_VERSION)
    list(APPEND _szx_openssl_args MIN_VERSION "${OpenSSL_FIND_VERSION}")
endif()
if(OpenSSL_FIND_COMPONENTS)
    list(APPEND _szx_openssl_args COMPONENTS ${OpenSSL_FIND_COMPONENTS})
endif()
szx_find_openssl(${_szx_openssl_args})

set(OpenSSL_Crypto_FOUND TRUE)
set(OpenSSL_SSL_FOUND TRUE)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSSL
    REQUIRED_VARS OPENSSL_INCLUDE_DIR OPENSSL_CRYPTO_LIBRARY OPENSSL_SSL_LIBRARY
    VERSION_VAR OpenSSL_VERSION
    HANDLE_COMPONENTS)
set(OPENSSL_FOUND "${OpenSSL_FOUND}")

unset(_szx_openssl_args)
