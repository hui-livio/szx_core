include_guard(GLOBAL)
include(szx_find_openssl)

# Single source of truth for the bundled libhv build.
#
# libhv is a private backend of szx::network. Application services, including
# szx_rbac_infra, consume szx::network instead of linking hv_static directly.
# Keep all backend build options here.

set(SZX_LIBHV_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/libhv" CACHE PATH
    "Bundled libhv source root")

option(SZX_NETWORK_ENABLE_HTTP_CLIENT "Build HTTP client support" ON)
option(SZX_NETWORK_ENABLE_TLS "Build libhv with OpenSSL TLS support" OFF)

function(szx_add_libhv)
    if(TARGET hv_static)
        return()
    endif()
    if(NOT EXISTS "${SZX_LIBHV_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "szx_add_libhv(): libhv source not found at ${SZX_LIBHV_ROOT}. "
            "Restore third_party/libhv or point SZX_LIBHV_ROOT at a checkout.")
    endif()

    # The options stay function-local: libhv consumes them as normal variables
    # while its subdirectory is configured (it sets CMP0077 to NEW), so the
    # parent project cache and later SZX components are not modified.  Same
    # approach as _szx_add_compression() in the top-level CMakeLists.
    #
    # The network component links hv_static, but BUILD_SHARED stays ON: on MSVC libhv
    # installs $<TARGET_PDB_FILE:hv> without guarding it on BUILD_SHARED
    # (third_party/libhv/CMakeLists.txt:276-278), so dropping the shared library
    # fails the generate step.  Changing that means patching vendored code.
    set(BUILD_SHARED ON)
    set(BUILD_STATIC ON)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_UNITTEST OFF)

    set(WITH_EVPP ON)
    set(WITH_HTTP ON)
    set(WITH_HTTP_SERVER ON)
    set(WITH_PROTOCOL OFF)
    set(WITH_MQTT OFF)
    set(WITH_KCP OFF)
    set(WITH_CURL OFF) # The adapter relies on libhv's native transport and TLS verification.

    # Applications may explicitly disable the client backend to minimize footprint.
    if(SZX_NETWORK_ENABLE_HTTP_CLIENT)
        set(WITH_HTTP_CLIENT ON)
    else()
        set(WITH_HTTP_CLIENT OFF)
    endif()

    if(SZX_NETWORK_ENABLE_TLS)
        szx_find_openssl(MIN_VERSION 3.5.8 COMPONENTS SSL)
        set(WITH_OPENSSL ON)
        set(OPENSSL_ROOT_DIR "${SZX_OPENSSL_ROOT}")
    else()
        set(WITH_OPENSSL OFF)
    endif()

    add_subdirectory("${SZX_LIBHV_ROOT}" "${CMAKE_BINARY_DIR}/third_party/libhv"
        EXCLUDE_FROM_ALL)
    set_target_properties(hv_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

    message(STATUS
        "libhv: consumers link hv_static, http_server=ON"
        ", http_client=${WITH_HTTP_CLIENT}"
        ", openssl=${WITH_OPENSSL}")
endfunction()
