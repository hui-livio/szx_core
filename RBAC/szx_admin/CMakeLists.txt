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

if(NOT TARGET szx::network)
    message(FATAL_ERROR "szx_admin_service requires SZX_COMPONENTS_BUILD_NETWORK=ON")
endif()

add_library(szx_rbac_infra STATIC
    src/Routes.cc
    src/infra/ErrorCode.cc
    src/infra/Database.cc
    src/infra/DatabaseInitialization.cc
    src/infra/HttpTransport.cc
    src/infra/RequestContext.cc
    src/infra/RestApi.cc
    src/infra/ServerConfig.cc
)
add_library(szx::rbac_infra ALIAS szx_rbac_infra)

target_include_directories(szx_rbac_infra
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(szx_rbac_infra
    PUBLIC szx::common
    PRIVATE szx::network szx::logger szx_rbac_ormpp szx::crypto szx::crypto_service
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

add_executable(szx_admin_service
    src/main.cc
)

target_link_libraries(szx_admin_service
    PRIVATE szx::rbac_infra szx::common szx::logger
)

target_compile_features(szx_admin_service PRIVATE cxx_std_20)
szx_apply_default_compile_policy(szx_admin_service)
szx_copy_runtime_dlls(szx_admin_service)

add_custom_command(TARGET szx_admin_service POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "$<TARGET_FILE_DIR:szx_admin_service>/config"
    COMMENT "Copying szx_admin_service configuration"
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web")
    add_custom_command(TARGET szx_admin_service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:szx_admin_service>/web"
        COMMENT "Copying szx_admin_service web assets"
    )
endif()
