cmake_minimum_required(VERSION 3.21)

project(SZXComponents VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(env)
include(git_hooks)
include(compile)
include(output)
include(util)
include(deps)
include(summary)
include(testing)

option(SZX_COMPONENTS_BUILD_TESTS "Build component smoke tests" ON)
option(SZX_COMPONENTS_INSTALL "Generate install tree and package config" ON)
option(SZX_COMPONENTS_BUILD_CAMERA "Build camera component" ON)
option(SZX_COMPONENTS_BUILD_PLC "Build PLC component when libmodbus is available" ON)
option(SZX_COMPONENTS_BUILD_IKAP "Build IKap adapter when SDK is available" OFF)
option(SZX_COMPONENTS_ENABLE_CPACK "Enable CPack package generation" OFF)
option(SZX_COMPONENTS_BUILD_HUARAY "Build Huaray camera adapters when SDK is available" ON)

set(SZX_IKAP_ROOT "" CACHE PATH "IKap SDK root (optional)")
set(SZX_OPENCV_DIR "" CACHE PATH "OpenCV package directory (optional)")
set(SZX_LIBMODBUS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmodbus" CACHE PATH "libmodbus root")
set(SZX_HUARAY_ROOT "" CACHE PATH "Huaray MV Viewer Development root")
set(SZX_HUARAY_APP_ROOT "" CACHE PATH "Huaray MV Viewer Application root")
szx_check_target_arch()
szx_setup_git_hooks()
szx_global_output_settings()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/CMakeLists.txt")
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/spdlog third_party/spdlog-build EXCLUDE_FROM_ALL)
else()
    find_package(spdlog CONFIG REQUIRED)
endif()

add_subdirectory(components/common)
add_subdirectory(components/logger)
add_subdirectory(components/crypto)

if(SZX_COMPONENTS_BUILD_CAMERA)
    add_subdirectory(components/camera)
endif()

if(SZX_COMPONENTS_BUILD_PLC)
    add_subdirectory(components/plc)
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
