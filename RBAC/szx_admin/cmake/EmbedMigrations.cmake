# SQL is a source input only. Normalize checkout line endings before compiling it.
set(_szx_migration_files
    "${CMAKE_CURRENT_SOURCE_DIR}/migrations/001_initial_schema.sql"
    "${CMAKE_CURRENT_SOURCE_DIR}/migrations/002_bootstrap_state.sql"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_szx_migration_files})
set(SZX_MIGRATION_ENTRIES "")
set(_szx_migration_version 0)
foreach(_szx_migration_file IN LISTS _szx_migration_files)
    math(EXPR _szx_migration_version "${_szx_migration_version} + 1")
    file(READ "${_szx_migration_file}" _szx_sql)
    string(REPLACE "\r\n" "\n" _szx_sql "${_szx_sql}")
    string(FIND "${_szx_sql}" ")SZXSQL\"" _szx_delimiter_position)
    if(NOT _szx_delimiter_position EQUAL -1)
        message(FATAL_ERROR "Migration contains the reserved C++ raw string delimiter")
    endif()
    string(SHA256 _szx_checksum "${_szx_sql}")
    string(APPEND SZX_MIGRATION_ENTRIES
        "    {${_szx_migration_version}, R\"SZXSQL(${_szx_sql})SZXSQL\", \"${_szx_checksum}\"},\n")
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbeddedMigrations.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedMigrations.h" @ONLY NEWLINE_STYLE UNIX)
target_include_directories(szx_rbac_infra PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
