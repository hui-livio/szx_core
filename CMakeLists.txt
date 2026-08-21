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

option(SZX_COMPONENTS_BUILD_TESTS "Build component smoke tests" ON)
option(SZX_COMPONENTS_INSTALL "Generate install tree and package config" ON)
option(SZX_COMPONENTS_BUILD_CAMERA "Build camera component" ON)
option(SZX_COMPONENTS_BUILD_PLC "Build PLC component when libmodbus is available" ON)
option(SZX_COMPONENTS_BUILD_FEATURE_MANAGER "Build the feature manager component" ON)
option(SZX_COMPONENTS_BUILD_UI_TOOLKITS "Build the Qt/QML UI toolkits component" ON)
option(SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP "Build the UIToolkits demo application" ON)
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
    set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt major version for SingleApplication")
    set(QAPPLICATION_CLASS QGuiApplication CACHE STRING "SingleApplication base class")
    add_subdirectory(third_party/SingleApplication third_party/SingleApplication-build EXCLUDE_FROM_ALL)
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
