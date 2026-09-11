add_library(szx_rbac_auth STATIC
    src/auth/JwtService.cc
)
add_library(szx::rbac_auth ALIAS szx_rbac_auth)

set(ENABLE_MYSQL ON)
set(BUILD_UNIT_TESTS OFF)
set(BUILD_EXAMPLES OFF)
if(NOT TARGET szx_rbac_ormpp)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../third_party/ormpp ${CMAKE_BINARY_DIR}/third_party/ormpp EXCLUDE_FROM_ALL)
    get_filename_component(_szx_mysql_library_dir "${MYSQL_LIBRARY}" DIRECTORY)
    if(WIN32)
        add_library(szx_rbac_mysqlclient SHARED IMPORTED GLOBAL)
        find_file(_szx_mysql_runtime NAMES libmysql.dll PATHS "${_szx_mysql_library_dir}" NO_DEFAULT_PATH)
        if(NOT _szx_mysql_runtime)
            message(FATAL_ERROR "MySQL import library was found, but libmysql.dll is missing beside it")
        endif()
        set_target_properties(szx_rbac_mysqlclient PROPERTIES IMPORTED_LOCATION "${_szx_mysql_runtime}")
    else()
        add_library(szx_rbac_mysqlclient UNKNOWN IMPORTED GLOBAL)
        set_target_properties(szx_rbac_mysqlclient PROPERTIES IMPORTED_LOCATION "${MYSQL_LIBRARY}")
    endif()
    set_target_properties(szx_rbac_mysqlclient PROPERTIES
        IMPORTED_IMPLIB "${MYSQL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_INCLUDE_DIR}")
    add_library(szx::rbac_mysqlclient ALIAS szx_rbac_mysqlclient)

    add_library(szx_rbac_ormpp INTERFACE)
    target_include_directories(szx_rbac_ormpp INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../../third_party/ormpp
        ${CMAKE_CURRENT_LIST_DIR}/../../third_party/ormpp/ormpp
    )
    target_compile_definitions(szx_rbac_ormpp INTERFACE ORMPP_ENABLE_MYSQL)
    target_link_libraries(szx_rbac_ormpp INTERFACE szx::rbac_mysqlclient)
endif()

set(BUILD_SHARED ON)
set(BUILD_STATIC ON)
set(BUILD_EXAMPLES OFF)
set(BUILD_UNITTEST OFF)
set(WITH_PROTOCOL OFF)
set(WITH_EVPP ON)
set(WITH_HTTP ON)
set(WITH_HTTP_SERVER ON)
set(WITH_HTTP_CLIENT OFF)
if(NOT TARGET hv_static)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../third_party/libhv ${CMAKE_BINARY_DIR}/third_party/libhv EXCLUDE_FROM_ALL)
endif()

add_library(szx_rbac_infra STATIC
    src/infra/ErrorCode.cc
    src/infra/Database.cc
    src/infra/HttpTransport.cc
    src/infra/RequestContext.cc
    src/infra/ServerConfig.cc
)
add_library(szx::rbac_infra ALIAS szx_rbac_infra)

target_include_directories(szx_rbac_infra
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(szx_rbac_infra
    PUBLIC szx::common
    PRIVATE hv_static szx_rbac_ormpp szx::crypto
)

target_compile_features(szx_rbac_infra PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_rbac_infra)

target_include_directories(szx_rbac_auth
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../third_party/jwt-cpp/include
        ${SZX_OPENSSL_ROOT}/include
)

target_link_libraries(szx_rbac_auth
    PUBLIC szx::crypto_service
)

target_compile_features(szx_rbac_auth PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_rbac_auth)
