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
if(NOT TARGET szx::metrics)
    message(FATAL_ERROR "szx_admin_service requires SZX_COMPONENTS_BUILD_METRICS=ON")
endif()

add_library(szx_admin_persistence STATIC
    src/infra/ErrorCode.cc
    src/infra/Database.cc
    src/infra/DatabaseUnitOfWork.cc
    src/infra/RoleRepository.cc
    src/infra/PermissionRepository.cc
    src/infra/OrmRuntime.cc
    src/infra/DatabaseInitialization.cc
    src/infra/AuditRepository.cc
    src/infra/BehaviorEventRepository.cc
    src/infra/LoginRepository.cc
    src/infra/LogMaintenanceRepository.cc
    src/infra/UserRepository.cc
    src/auth/Identifier.cc
    src/auth/PasswordHash.cc
)
add_library(szx_admin_application STATIC
    src/auth/ChangePassword.cc
    src/auth/SessionLifecycle.cc
    src/auth/LoginService.cc
    src/rbac/AuthorizationService.cc
    src/rbac/RoleService.cc
    src/rbac/PermissionService.cc
    src/audit/AuditService.cc
    src/telemetry/BehaviorEventService.cc
    src/telemetry/BehaviorEventWriter.cc
    src/maintenance/LogMaintenanceService.cc
    src/user/UserService.cc
    src/user/UserLifecycle.cc
    src/policy/Policy.cc
    src/system/SystemMetrics.cc
)
add_library(szx_admin_http STATIC
    src/Routes.cc
    src/system/SystemMetricsHandler.cc
    src/HttpSupport.cc
    src/rbac/RbacHandler.cc
    src/infra/HttpTransport.cc
    src/infra/RequestContext.cc
    src/infra/RestApi.cc
    src/infra/ServerConfig.cc
    src/user/UserHandler.cc
)
add_library(szx_rbac_infra INTERFACE)
add_library(szx::rbac_infra ALIAS szx_rbac_infra)
add_library(szx::admin_persistence ALIAS szx_admin_persistence)
add_library(szx::admin_application ALIAS szx_admin_application)
add_library(szx::admin_http ALIAS szx_admin_http)

target_include_directories(szx_admin_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(szx_admin_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(szx_admin_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(szx_admin_persistence PUBLIC cxx_std_20)
target_compile_features(szx_admin_application PUBLIC cxx_std_20)
target_compile_features(szx_admin_http PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_admin_persistence)
szx_apply_default_compile_policy(szx_admin_application)
szx_apply_default_compile_policy(szx_admin_http)

target_link_libraries(szx_admin_persistence
    PUBLIC szx::common
    PRIVATE szx::logger szx_rbac_ormpp szx::crypto szx::crypto_service szx::rbac_auth
)
target_link_libraries(szx_admin_application PUBLIC szx::common szx::network szx_admin_persistence
    PRIVATE szx::logger szx::crypto szx::crypto_service szx::rbac_auth szx::metrics)
target_link_libraries(szx_admin_http PUBLIC szx::common szx::network szx_admin_application szx_admin_persistence
    PRIVATE szx::logger szx_rbac_auth)
target_link_libraries(szx_rbac_infra INTERFACE szx_admin_http szx_admin_application szx_admin_persistence)

if(WIN32)
    target_link_libraries(szx_admin_persistence PRIVATE Normaliz)
else()
    find_package(ICU REQUIRED COMPONENTS uc)
    target_link_libraries(szx_admin_persistence PRIVATE ICU::uc)
endif()

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
    PRIVATE szx::rbac_infra szx::rbac_auth szx::common szx::logger szx::metrics
)

target_compile_features(szx_admin_service PRIVATE cxx_std_20)
szx_apply_default_compile_policy(szx_admin_service)
szx_copy_runtime_dlls(szx_admin_service)

# Install locked frontend dependencies, build the production bundle, and replace
# deployed assets after linking the service.
if(WIN32)
    find_program(_SZX_ADMIN_NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED NO_CACHE)
else()
    find_program(_SZX_ADMIN_NPM_EXECUTABLE NAMES npm REQUIRED NO_CACHE)
endif()

add_custom_command(TARGET szx_admin_service POST_BUILD
    COMMAND "${_SZX_ADMIN_NPM_EXECUTABLE}" ci --no-audit --no-fund
    COMMAND "${_SZX_ADMIN_NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/admin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "$<TARGET_FILE_DIR:szx_admin_service>/config"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
    "$<TARGET_FILE_DIR:szx_admin_service>/web/admin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/web/admin/dist"
    "$<TARGET_FILE_DIR:szx_admin_service>/web/admin"
    COMMENT "Installing frontend dependencies, building admin web, and replacing deployed assets"
    VERBATIM
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web/api-docs/index.html")
    add_custom_command(TARGET szx_admin_service POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:szx_admin_service>/web/api-docs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web/api-docs"
        "$<TARGET_FILE_DIR:szx_admin_service>/web/api-docs"
        COMMENT "Copying szx_admin_service API documentation assets"
        VERBATIM
    )
endif()
