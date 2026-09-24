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
    src/HttpSupport.cc
    src/auth/ChangePassword.cc
    src/auth/SessionLifecycle.cc
    src/auth/Identifier.cc
    src/auth/LoginService.cc
    src/auth/PasswordHash.cc
    src/rbac/AuthorizationService.cc
    src/audit/AuditService.cc
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
    src/infra/HttpTransport.cc
    src/infra/RequestContext.cc
    src/infra/RestApi.cc
    src/infra/ServerConfig.cc
    src/policy/Policy.cc
    src/telemetry/BehaviorEventService.cc
    src/telemetry/BehaviorEventWriter.cc
    src/maintenance/LogMaintenanceService.cc
    src/user/UserHandler.cc
    src/user/UserService.cc
    src/user/UserLifecycle.cc
)
add_library(szx::rbac_infra ALIAS szx_rbac_infra)

target_include_directories(szx_rbac_infra
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(szx_rbac_infra
    PUBLIC szx::common szx::network
    PRIVATE szx::logger szx_rbac_ormpp szx::crypto szx::crypto_service szx::rbac_auth
)

if(WIN32)
    target_link_libraries(szx_rbac_infra PRIVATE Normaliz)
else()
    find_package(ICU REQUIRED COMPONENTS uc)
    target_link_libraries(szx_rbac_infra PRIVATE ICU::uc)
endif()

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
    PRIVATE szx::rbac_infra szx::rbac_auth szx::common szx::logger
)

target_compile_features(szx_admin_service PRIVATE cxx_std_20)
szx_apply_default_compile_policy(szx_admin_service)
szx_copy_runtime_dlls(szx_admin_service)

# Build the admin frontend and deploy the freshly generated production bundle
# whenever the service target is built, including builds without relinking.
if(WIN32)
    find_program(_SZX_ADMIN_NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED NO_CACHE)
else()
    find_program(_SZX_ADMIN_NPM_EXECUTABLE NAMES npm REQUIRED NO_CACHE)
endif()
add_custom_target(szx_admin_web_assets
    COMMAND "${_SZX_ADMIN_NPM_EXECUTABLE}" run build
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/web/admin/dist"
    "$<TARGET_FILE_DIR:szx_admin_service>/web/admin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/admin"
    COMMENT "Building and deploying admin frontend at /admin"
    VERBATIM
)
add_dependencies(szx_admin_service szx_admin_web_assets)

add_custom_command(TARGET szx_admin_service POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "$<TARGET_FILE_DIR:szx_admin_service>/config"
    COMMENT "Copying szx_admin_service configuration"
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web/api-docs/index.html")
    add_custom_target(szx_admin_service_web_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web/api-docs"
        "$<TARGET_FILE_DIR:szx_admin_service>/web/api-docs"
        COMMENT "Copying szx_admin_service API documentation assets"
        VERBATIM
    )
    add_dependencies(szx_admin_service szx_admin_service_web_assets)
endif()
