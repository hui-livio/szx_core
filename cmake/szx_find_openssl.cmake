include_guard(GLOBAL)

set(SZX_OPENSSL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/OpenSSL" CACHE PATH
    "Bundled OpenSSL SDK root")
set(SZX_OPENSSL_LINKAGE "STATIC" CACHE STRING
    "Bundled OpenSSL linkage (STATIC or SHARED)")
set_property(CACHE SZX_OPENSSL_LINKAGE PROPERTY STRINGS STATIC SHARED)
set(SZX_OPENSSL_MSVC_RUNTIME "MD" CACHE STRING
    "MSVC runtime used by the bundled OpenSSL libraries (MD or MT)")
set_property(CACHE SZX_OPENSSL_MSVC_RUNTIME PROPERTY STRINGS MD MT)

function(szx_find_openssl)
    cmake_parse_arguments(ARG "" "MIN_VERSION" "COMPONENTS" ${ARGN})

    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "The bundled OpenSSL SDK supports Windows MSVC x64 only")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The bundled OpenSSL SDK requires a 64-bit target")
    endif()

    string(TOUPPER "${SZX_OPENSSL_LINKAGE}" _linkage)
    if(NOT _linkage STREQUAL "STATIC" AND NOT _linkage STREQUAL "SHARED")
        message(FATAL_ERROR
            "SZX_OPENSSL_LINKAGE must be STATIC or SHARED, got '${SZX_OPENSSL_LINKAGE}'")
    endif()
    string(TOUPPER "${SZX_OPENSSL_MSVC_RUNTIME}" _runtime)
    if(NOT _runtime STREQUAL "MD" AND NOT _runtime STREQUAL "MT")
        message(FATAL_ERROR
            "SZX_OPENSSL_MSVC_RUNTIME must be MD or MT, got '${SZX_OPENSSL_MSVC_RUNTIME}'")
    endif()
    set(SZX_OPENSSL_LINKAGE "${_linkage}" CACHE STRING
        "Bundled OpenSSL linkage (STATIC or SHARED)" FORCE)
    set_property(CACHE SZX_OPENSSL_LINKAGE PROPERTY STRINGS STATIC SHARED)
    set(SZX_OPENSSL_MSVC_RUNTIME "${_runtime}" CACHE STRING
        "MSVC runtime used by the bundled OpenSSL libraries (MD or MT)" FORCE)
    set_property(CACHE SZX_OPENSSL_MSVC_RUNTIME PROPERTY STRINGS MD MT)
    if(_runtime STREQUAL "MT")
        if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
            message(FATAL_ERROR
                "SZX_OPENSSL_MSVC_RUNTIME=MT requires CMAKE_MSVC_RUNTIME_LIBRARY to select "
                "the static MSVC runtime")
        elseif(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
            message(FATAL_ERROR
                "SZX_OPENSSL_MSVC_RUNTIME=MT conflicts with "
                "CMAKE_MSVC_RUNTIME_LIBRARY='${CMAKE_MSVC_RUNTIME_LIBRARY}'")
        endif()
    elseif(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        message(FATAL_ERROR
            "SZX_OPENSSL_MSVC_RUNTIME=MD conflicts with "
            "CMAKE_MSVC_RUNTIME_LIBRARY='${CMAKE_MSVC_RUNTIME_LIBRARY}'")
    endif()

    get_filename_component(_root "${SZX_OPENSSL_ROOT}" ABSOLUTE)
    set(_include_dir "${_root}/include")
    set(_version_header "${_include_dir}/openssl/opensslv.h")
    if(NOT EXISTS "${_version_header}")
        message(FATAL_ERROR "OpenSSL version header not found: ${_version_header}")
    endif()

    file(READ "${_version_header}" _version_contents)
    string(REGEX MATCH
        "#[ \t]*define[ \t]+OPENSSL_VERSION_MAJOR[ \t]+([0-9]+)"
        _major_match "${_version_contents}")
    set(_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH
        "#[ \t]*define[ \t]+OPENSSL_VERSION_MINOR[ \t]+([0-9]+)"
        _minor_match "${_version_contents}")
    set(_minor "${CMAKE_MATCH_1}")
    string(REGEX MATCH
        "#[ \t]*define[ \t]+OPENSSL_VERSION_PATCH[ \t]+([0-9]+)"
        _patch_match "${_version_contents}")
    set(_patch "${CMAKE_MATCH_1}")
    if(_major STREQUAL "" OR _minor STREQUAL "" OR _patch STREQUAL "")
        message(FATAL_ERROR "Unable to parse OpenSSL version from ${_version_header}")
    endif()
    set(_version "${_major}.${_minor}.${_patch}")
    if(ARG_MIN_VERSION)
        set(_minimum_version "${ARG_MIN_VERSION}")
    else()
        set(_minimum_version "3.5.8")
    endif()
    if(_version VERSION_LESS _minimum_version)
        message(FATAL_ERROR
            "Bundled OpenSSL ${_version} is older than required ${_minimum_version}")
    endif()

    set(_library_dir "${_root}/lib/VC/x64/${_runtime}")
    if(_linkage STREQUAL "STATIC")
        set(_crypto_library "${_library_dir}/libcrypto_static.lib")
        set(_ssl_library "${_library_dir}/libssl_static.lib")
        set(_crypto_dll "")
        set(_ssl_dll "")
        set(_imported_type STATIC)
    else()
        set(_crypto_library "${_library_dir}/libcrypto.lib")
        set(_ssl_library "${_library_dir}/libssl.lib")
        set(_crypto_dll "${_root}/bin/libcrypto-3-x64.dll")
        set(_ssl_dll "${_root}/bin/libssl-3-x64.dll")
        set(_imported_type SHARED)
    endif()

    foreach(_required IN ITEMS "${_crypto_library}" "${_ssl_library}")
        if(NOT EXISTS "${_required}")
            message(FATAL_ERROR "Bundled OpenSSL library not found: ${_required}")
        endif()
    endforeach()
    if(_linkage STREQUAL "SHARED")
        foreach(_required IN ITEMS "${_crypto_dll}" "${_ssl_dll}")
            if(NOT EXISTS "${_required}")
                message(FATAL_ERROR "Bundled OpenSSL runtime not found: ${_required}")
            endif()
        endforeach()
    endif()

    if((TARGET OpenSSL::Crypto OR TARGET OpenSSL::SSL) AND
       (NOT TARGET SZX::OpenSSL::Crypto OR NOT TARGET SZX::OpenSSL::SSL))
        message(FATAL_ERROR
            "OpenSSL targets already exist outside the bundled SZX OpenSSL integration")
    endif()

    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto ${_imported_type} IMPORTED GLOBAL)
        add_library(OpenSSL::SSL ${_imported_type} IMPORTED GLOBAL)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES C
            INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}")
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES C
            INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
            INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
        if(_linkage STREQUAL "STATIC")
            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION "${_crypto_library}"
                INTERFACE_LINK_LIBRARIES "ws2_32;gdi32;advapi32;crypt32;user32")
            set_target_properties(OpenSSL::SSL PROPERTIES
                IMPORTED_LOCATION "${_ssl_library}")
        else()
            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_IMPLIB "${_crypto_library}"
                IMPORTED_LOCATION "${_crypto_dll}")
            set_target_properties(OpenSSL::SSL PROPERTIES
                IMPORTED_IMPLIB "${_ssl_library}"
                IMPORTED_LOCATION "${_ssl_dll}")
        endif()

        add_library(SZX::OpenSSL::Crypto INTERFACE IMPORTED GLOBAL)
        set_target_properties(SZX::OpenSSL::Crypto PROPERTIES
            INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
        add_library(SZX::OpenSSL::SSL INTERFACE IMPORTED GLOBAL)
        set_target_properties(SZX::OpenSSL::SSL PROPERTIES
            INTERFACE_LINK_LIBRARIES OpenSSL::SSL)

        message(STATUS
            "Bundled OpenSSL ${_version}: ${_linkage}, ${_runtime}, ${_root}")
    endif()

    set(OpenSSL_FOUND TRUE PARENT_SCOPE)
    set(OPENSSL_FOUND TRUE PARENT_SCOPE)
    set(OpenSSL_VERSION "${_version}" PARENT_SCOPE)
    set(OPENSSL_VERSION "${_version}" PARENT_SCOPE)
    set(OPENSSL_VERSION_MAJOR "${_major}" PARENT_SCOPE)
    set(OPENSSL_VERSION_MINOR "${_minor}" PARENT_SCOPE)
    set(OPENSSL_VERSION_FIX "${_patch}" PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIR "${_include_dir}" PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIRS "${_include_dir}" PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARY "${_crypto_library}" PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARIES "${_crypto_library}" PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARY "${_ssl_library}" PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARIES "${_ssl_library};OpenSSL::Crypto" PARENT_SCOPE)
    set(OPENSSL_LIBRARIES "${_ssl_library};${_crypto_library}" PARENT_SCOPE)
    set(OPENSSL_RUNTIME_DIR "${_root}/bin" PARENT_SCOPE)
    if(_linkage STREQUAL "STATIC")
        set(OPENSSL_USE_STATIC_LIBS TRUE PARENT_SCOPE)
    else()
        set(OPENSSL_USE_STATIC_LIBS FALSE PARENT_SCOPE)
    endif()
endfunction()

function(szx_copy_openssl_runtime_dlls _target)
    if(NOT TARGET "${_target}" OR NOT SZX_OPENSSL_LINKAGE STREQUAL "SHARED")
        return()
    endif()
    foreach(_dll IN ITEMS libcrypto-3-x64.dll libssl-3-x64.dll)
        add_custom_command(TARGET "${_target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${SZX_OPENSSL_ROOT}/bin/${_dll}" "$<TARGET_FILE_DIR:${_target}>"
            COMMENT "Copying ${_dll} for ${_target}"
            VERBATIM)
    endforeach()
endfunction()

function(szx_install_openssl_runtime)
    install(FILES "${SZX_OPENSSL_ROOT}/LICENSE.txt"
        DESTINATION licenses/OpenSSL
        COMPONENT runtime)
    if(SZX_OPENSSL_LINKAGE STREQUAL "SHARED")
        install(FILES
            "${SZX_OPENSSL_ROOT}/bin/libcrypto-3-x64.dll"
            "${SZX_OPENSSL_ROOT}/bin/libssl-3-x64.dll"
            DESTINATION "${SZX_INSTALL_BINDIR}"
            COMPONENT runtime)
    endif()
endfunction()
