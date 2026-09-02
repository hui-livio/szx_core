cmake_minimum_required(VERSION 3.21)

project(SZXComponents VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(_SZX_CORE_PARENT_MODULE_PATH "${CMAKE_MODULE_PATH}")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(comm_env)
include(comm_git_hooks)
include(comm_compile)
include(comm_output)
include(comm_util)
include(core_deps)
include(core_summary)
include(comm_testing)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SZX_COMPONENTS_STANDALONE_BUILD TRUE)
    set(_SZX_COMPONENTS_DEFAULT_TESTS ON)
    set(_SZX_COMPONENTS_DEFAULT_INSTALL ON)
    set(_SZX_COMPONENTS_DEFAULT_CAMERA ON)
    set(_SZX_COMPONENTS_DEFAULT_PLC ON)
    set(_SZX_COMPONENTS_DEFAULT_FEATURE_MANAGER ON)
    set(_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS ON)
    set(_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS_APP ON)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd/build/cmake/CMakeLists.txt")
        set(_SZX_COMPONENTS_DEFAULT_COMPRESSION ON)
    else()
        # Keep a source checkout buildable when the optional zstd CMake tree
        # is absent from a shallow or partial checkout.
        set(_SZX_COMPONENTS_DEFAULT_COMPRESSION OFF)
    endif()
else()
    set(SZX_COMPONENTS_STANDALONE_BUILD FALSE)
    # A subproject must not add tests, install rules, demo applications, or
    # optional hardware/compression dependencies to its host by default.
    set(_SZX_COMPONENTS_DEFAULT_TESTS OFF)
    set(_SZX_COMPONENTS_DEFAULT_INSTALL OFF)
    set(_SZX_COMPONENTS_DEFAULT_CAMERA OFF)
    set(_SZX_COMPONENTS_DEFAULT_PLC OFF)
    set(_SZX_COMPONENTS_DEFAULT_FEATURE_MANAGER OFF)
    set(_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS OFF)
    set(_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS_APP OFF)
    set(_SZX_COMPONENTS_DEFAULT_COMPRESSION OFF)
endif()

option(SZX_COMPONENTS_BUILD_TESTS "Build component smoke tests" ${_SZX_COMPONENTS_DEFAULT_TESTS})
option(SZX_COMPONENTS_INSTALL "Generate install tree and package config" ${_SZX_COMPONENTS_DEFAULT_INSTALL})
option(SZX_COMPONENTS_BUILD_CAMERA "Build camera component" ${_SZX_COMPONENTS_DEFAULT_CAMERA})
option(SZX_COMPONENTS_BUILD_PLC "Build PLC component when libmodbus is available" ${_SZX_COMPONENTS_DEFAULT_PLC})
option(SZX_COMPONENTS_BUILD_FEATURE_MANAGER "Build the feature manager component" ${_SZX_COMPONENTS_DEFAULT_FEATURE_MANAGER})
option(SZX_COMPONENTS_BUILD_UI_TOOLKITS "Build the Qt/QML UI toolkits component" ${_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS})
option(SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP "Build the UIToolkits demo application" ${_SZX_COMPONENTS_DEFAULT_UI_TOOLKITS_APP})
option(SZX_COMPONENTS_BUILD_IKAP "Build IKap adapter when SDK is available" OFF)
option(SZX_COMPONENTS_ENABLE_CPACK "Enable CPack package generation" OFF)
option(SZX_COMPONENTS_BUILD_HUARAY "Build Huaray camera adapters when SDK is available" ON)
option(SZX_COMPONENTS_BUILD_COMPRESSION "Build bundled ZIP/Zstandard compression stack" ${_SZX_COMPONENTS_DEFAULT_COMPRESSION})
set(SZX_COMPRESSION_BACKEND "minizip-ng" CACHE STRING
    "ZIP backend used by szx::compression (minizip-ng or none)")
set_property(CACHE SZX_COMPRESSION_BACKEND PROPERTY STRINGS minizip-ng none)

set(SZX_IKAP_ROOT "" CACHE PATH "IKap SDK root (optional)")
set(SZX_OPENCV_DIR "" CACHE PATH "OpenCV package directory (optional)")
set(SZX_LIBMODBUS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmodbus" CACHE PATH "libmodbus root")
set(SZX_HUARAY_ROOT "" CACHE PATH "Huaray MV Viewer Development root")
set(SZX_HUARAY_APP_ROOT "" CACHE PATH "Huaray MV Viewer Application root")
szx_check_target_arch()
szx_setup_git_hooks()
if(SZX_COMPONENTS_STANDALONE_BUILD)
    szx_global_output_settings()
endif()

if(TARGET spdlog::spdlog OR TARGET spdlog)
    message(STATUS "Using host-provided spdlog target")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/CMakeLists.txt")
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/spdlog third_party/spdlog-build EXCLUDE_FROM_ALL)
else()
    find_package(spdlog CONFIG REQUIRED)
endif()

function(_szx_add_compression)
    # Keep compression options local to this function.  The bundled projects
    # consume normal variables while configuring their subdirectories, so the
    # parent project cache and later SZX components are not modified.
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)

    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_TESTS OFF)
    set(ZSTD_BUILD_CONTRIB OFF)
    set(ZSTD_BUILD_STATIC ON)
    set(ZSTD_BUILD_SHARED OFF)
    add_subdirectory(third_party/zstd/build/cmake third_party/zstd-build EXCLUDE_FROM_ALL)
    if(TARGET libzstd_static AND NOT TARGET zstd::libzstd_static)
        add_library(zstd::libzstd_static ALIAS libzstd_static)
    endif()
    if(TARGET libzstd_shared AND NOT TARGET zstd::libzstd_shared)
        add_library(zstd::libzstd_shared ALIAS libzstd_shared)
    endif()

    set(ZLIB_COMPAT OFF)
    set(WITH_GTEST OFF)
    set(WITH_FUZZERS OFF)
    set(WITH_BENCHMARKS OFF)
    set(WITH_BENCHMARK_APPS OFF)
    add_subdirectory(third_party/zlib-ng third_party/zlib-ng-build EXCLUDE_FROM_ALL)

    # minizip-ng discovers compression backends through package config files.
    # These tiny build-tree configs point it at the already-created local
    # targets, preventing an accidental fallback to a host installation or a
    # network FetchContent clone.
    set(_SZX_COMPRESSION_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/compression-packages")
    file(MAKE_DIRECTORY "${_SZX_COMPRESSION_PACKAGE_DIR}")
    file(WRITE "${_SZX_COMPRESSION_PACKAGE_DIR}/ZLIB-NGConfig.cmake"
        "set(ZLIB-NG_FOUND TRUE)\nset(ZLIB-NG_VERSION 2.3.3)\n")
    file(WRITE "${_SZX_COMPRESSION_PACKAGE_DIR}/zstdConfig.cmake"
        "set(zstd_FOUND TRUE)\nset(zstd_VERSION 1.5.7)\n")
    list(PREPEND CMAKE_PREFIX_PATH "${_SZX_COMPRESSION_PACKAGE_DIR}")

    set(MZ_COMPAT OFF)
    set(MZ_ZLIB ON)
    set(MZ_ZLIB_FLAVOR "zlib-ng")
    set(MZ_ZSTD ON)
    set(MZ_BZIP2 OFF)
    set(MZ_LZMA OFF)
    set(MZ_PPMD OFF)
    set(MZ_OPENSSL OFF)
    set(MZ_ICONV OFF)
    set(MZ_FETCH_LIBS OFF)
    set(MZ_BUILD_TESTS OFF)
    set(MZ_BUILD_UNIT_TESTS OFF)
    set(MZ_BUILD_FUZZ_TESTS OFF)
    add_subdirectory(third_party/minizip-ng third_party/minizip-ng-build EXCLUDE_FROM_ALL)

endfunction()

if(SZX_COMPONENTS_BUILD_COMPRESSION AND SZX_COMPRESSION_BACKEND STREQUAL "minizip-ng"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd/build/cmake/CMakeLists.txt")
    _szx_add_compression()
elseif(SZX_COMPONENTS_BUILD_COMPRESSION AND SZX_COMPRESSION_BACKEND STREQUAL "minizip-ng")
    message(FATAL_ERROR
        "SZX_COMPONENTS_BUILD_COMPRESSION=ON requires the bundled zstd CMake project at "
        "third_party/zstd/build/cmake. Restore that directory or set "
        "SZX_COMPONENTS_BUILD_COMPRESSION=OFF.")
elseif(SZX_COMPONENTS_BUILD_COMPRESSION AND NOT SZX_COMPRESSION_BACKEND STREQUAL "none")
    message(FATAL_ERROR
        "Unsupported SZX_COMPRESSION_BACKEND='${SZX_COMPRESSION_BACKEND}'; "
        "expected 'minizip-ng' or 'none'.")
endif()

add_subdirectory(components/common)
add_subdirectory(components/logger)
add_subdirectory(components/crypto)
add_subdirectory(components/keystore)
if(SZX_COMPONENTS_BUILD_COMPRESSION)
    add_subdirectory(components/compression)
endif()

if(SZX_COMPONENTS_BUILD_FEATURE_MANAGER)
    add_subdirectory(components/feature_manager)
endif()

if(SZX_COMPONENTS_BUILD_CAMERA)
    add_subdirectory(components/camera)
endif()

if(SZX_COMPONENTS_BUILD_PLC)
    add_subdirectory(components/plc)
endif()

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS)
    add_subdirectory(components/UIToolkits)
endif()

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP)
    if(NOT SZX_COMPONENTS_BUILD_UI_TOOLKITS)
        message(FATAL_ERROR "SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP requires SZX_COMPONENTS_BUILD_UI_TOOLKITS=ON")
    endif()
    add_subdirectory(tools/UIToolkitsApp)
endif()

szx_install_components_package()

if(SZX_COMPONENTS_BUILD_TESTS)
    enable_testing()
    szx_find_gtest()
    add_subdirectory(tests)

endif()

if(SZX_COMPONENTS_ENABLE_CPACK AND SZX_COMPONENTS_INSTALL)
    szx_setup_packaging(VENDOR "SZX" DESCRIPTION "SZX Components SDK" COMPONENTS runtime devel)
endif()

szx_print_config_summary()

# Do not leak core helper-module precedence into the product repository.
set(CMAKE_MODULE_PATH "${_SZX_CORE_PARENT_MODULE_PATH}")
unset(_SZX_CORE_PARENT_MODULE_PATH)
