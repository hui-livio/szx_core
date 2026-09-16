include_guard(GLOBAL)

# Single source of truth for the bundled libhv build.
#
# libhv is consumed by szx_rbac_infra and szx::network.
# Each consumer used to set its own libhv options right before calling
# add_subdirectory(), so whichever directory was configured first silently
# decided what the shared hv_static target contained; a consumer that expected
# WITH_HTTP_CLIENT=ON would only find out at link time.  Every consumer calls
# szx_add_libhv() instead, and the options below are the only place they live.

set(SZX_LIBHV_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/libhv" CACHE PATH
    "Bundled libhv source root")

option(SZX_NETWORK_ENABLE_HTTP_CLIENT "Build libhv with HTTP client support" OFF)
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
    # Every consumer links hv_static, but BUILD_SHARED stays ON: on MSVC libhv
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

    # Off until szx::network actually ships HttpClient, so the admin service
    # keeps its current footprint.  Flipping this option is the whole point of
    # having one call site: both consumers move together.
    if(SZX_NETWORK_ENABLE_HTTP_CLIENT)
        set(WITH_HTTP_CLIENT ON)
    else()
        set(WITH_HTTP_CLIENT OFF)
    endif()

    if(SZX_NETWORK_ENABLE_TLS)
        set(WITH_OPENSSL ON)
        # SZX_OPENSSL_ROOT is the cache path components/crypto defines, but that
        # only happens once crypto has been configured.  Fall back to the same
        # bundled SDK so this does not depend on subdirectory order.
        if(SZX_OPENSSL_ROOT)
            set(_szx_openssl_root "${SZX_OPENSSL_ROOT}")
        else()
            set(_szx_openssl_root
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../third_party/OpenSSL/WIN64_Static")
        endif()
        set(OPENSSL_ROOT_DIR "${_szx_openssl_root}")
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
