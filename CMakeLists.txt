cmake_minimum_required(VERSION 3.21)

project(szx_core
    VERSION 0.1.0
    DESCRIPTION "Common library for industrial vision IO: multi-vendor camera SDK abstraction, PLC Modbus control, structured logging."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif ()

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# =====================================================================
# Options
# =====================================================================
option(SZX_CORE_BUILD_EXAMPLES   "Build example programs"                ON)
option(SZX_CORE_BUILD_TESTS      "Build smoke tests"                     ON)
option(SZX_CORE_BUILD_SHARED     "Build shared (DLL) libraries"          OFF)
option(SZX_CORE_FETCH_SPDLOG     "Fetch spdlog via FetchContent if not found locally" ON)
option(SZX_CORE_INSTALL          "Generate install rules and CMake package config (off by default; turn on for find_package consumption)" OFF)

# 厂商 SDK 路径，可在命令行覆盖 -DIKAP_ROOT=... 等
set(IKAP_ROOT     "C:/Program Files/I-TEK OptoElectronics/IKapLibrary" CACHE PATH "Root of IKap SDK (optional)")
set(HUARAY_ROOT   "D:/HuarayTech/MV Viewer/Development"                CACHE PATH "Root of HuarayTech MV Viewer Development (optional)")
set(HUARAY_APP_ROOT "D:/HuarayTech/MV Viewer/Application"              CACHE PATH "Root of HuarayTech MV Viewer Application (runtime dlls)")
set(LIBMODBUS_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmodbus" CACHE PATH "Root of libmodbus (include/lib/bin)")

if (SZX_CORE_BUILD_SHARED)
    set(BUILD_SHARED_LIBS ON)
endif ()

# =====================================================================
# Dependency: spdlog (required for common_support)
# =====================================================================
find_package(spdlog CONFIG QUIET)
if (NOT TARGET spdlog::spdlog AND NOT TARGET spdlog::spdlog_header_only)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog"
                         "${CMAKE_CURRENT_BINARY_DIR}/spdlog")
    elseif (SZX_CORE_FETCH_SPDLOG)
        message(STATUS "spdlog not found locally; fetching via FetchContent...")
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.14.1
        )
        FetchContent_MakeAvailable(spdlog)
    else ()
        message(FATAL_ERROR "spdlog not found and SZX_CORE_FETCH_SPDLOG=OFF")
    endif ()
endif ()

# =====================================================================
# Optional dependency: OpenCV (required only by FileImage fake-camera adapter)
# =====================================================================
set(SZX_CORE_HAS_OPENCV OFF)
find_package(OpenCV CONFIG QUIET COMPONENTS core imgcodecs imgproc)
if (OpenCV_FOUND)
    set(SZX_CORE_HAS_OPENCV ON)
endif ()

if (SZX_CORE_HAS_OPENCV)
    message(STATUS "OpenCV detected: ${OpenCV_VERSION}")
else ()
    message(STATUS "OpenCV not detected; FileImage fake-camera adapter will NOT be built. Set -DOpenCV_DIR=...")
endif ()

# =====================================================================
# Optional dependency: IKap SDK
# =====================================================================
set(SZX_CORE_HAS_IKAP OFF)
if (WIN32 AND EXISTS "${IKAP_ROOT}")
    set(IKAP_INCLUDE_DIR "${IKAP_ROOT}/Include")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IKAP_LIB_DIR "${IKAP_ROOT}/Lib/x64")
        set(IKAP_BIN_DIR "${IKAP_ROOT}/Dll/C/x64")
    else ()
        set(IKAP_LIB_DIR "${IKAP_ROOT}/Lib/x86")
        set(IKAP_BIN_DIR "${IKAP_ROOT}/Dll/C/x86")
    endif ()

    if (EXISTS "${IKAP_INCLUDE_DIR}/IKapC.h" AND
        EXISTS "${IKAP_LIB_DIR}/IKapC.lib" AND
        EXISTS "${IKAP_LIB_DIR}/IKapBoard.lib")
        set(SZX_CORE_HAS_IKAP ON)
        add_library(IKap::IKapC SHARED IMPORTED GLOBAL)
        set_target_properties(IKap::IKapC PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${IKAP_INCLUDE_DIR}"
            IMPORTED_IMPLIB "${IKAP_LIB_DIR}/IKapC.lib"
        )
        if (EXISTS "${IKAP_BIN_DIR}/IKapC.dll")
            set_target_properties(IKap::IKapC PROPERTIES
                IMPORTED_LOCATION "${IKAP_BIN_DIR}/IKapC.dll")
        endif ()

        add_library(IKap::IKapBoard SHARED IMPORTED GLOBAL)
        set_target_properties(IKap::IKapBoard PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${IKAP_INCLUDE_DIR}"
            IMPORTED_IMPLIB "${IKAP_LIB_DIR}/IKapBoard.lib"
        )
        if (EXISTS "${IKAP_BIN_DIR}/IKapBoard.dll")
            set_target_properties(IKap::IKapBoard PROPERTIES
                IMPORTED_LOCATION "${IKAP_BIN_DIR}/IKapBoard.dll")
        endif ()
    endif ()
endif ()

if (SZX_CORE_HAS_IKAP)
    message(STATUS "IKap SDK detected: ${IKAP_ROOT}")
else ()
    message(STATUS "IKap SDK not detected; IKap adapter will not be built. Set -DIKAP_ROOT=...")
endif ()

# =====================================================================
# Optional dependency: HuarayTech SDK (IMVFG + IMV share MVSDKmd.lib)
# =====================================================================
set(SZX_CORE_HAS_HUARAY OFF)
if (WIN32 AND EXISTS "${HUARAY_ROOT}")
    set(HUARAY_INCLUDE_FG  "${HUARAY_ROOT}/Include/IMVFG")
    set(HUARAY_INCLUDE_IMV "${HUARAY_ROOT}/Include/IMV")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(HUARAY_LIB_DIR "${HUARAY_ROOT}/Lib/x64")
        set(HUARAY_BIN_DIR "${HUARAY_APP_ROOT}/x64")
    else ()
        set(HUARAY_LIB_DIR "${HUARAY_ROOT}/Lib/win32")
        set(HUARAY_BIN_DIR "${HUARAY_APP_ROOT}/win32")
    endif ()

    if (EXISTS "${HUARAY_INCLUDE_FG}/IMVFGApi.h" AND
        EXISTS "${HUARAY_INCLUDE_IMV}/IMVApi.h"  AND
        EXISTS "${HUARAY_LIB_DIR}/MVSDKmd.lib")
        set(SZX_CORE_HAS_HUARAY ON)
        add_library(Huaray::IMVFG SHARED IMPORTED GLOBAL)
        set_target_properties(Huaray::IMVFG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${HUARAY_INCLUDE_FG};${HUARAY_INCLUDE_IMV}"
            IMPORTED_IMPLIB "${HUARAY_LIB_DIR}/MVSDKmd.lib"
        )
        if (EXISTS "${HUARAY_BIN_DIR}/MVSDKmd.dll")
            set_target_properties(Huaray::IMVFG PROPERTIES
                IMPORTED_LOCATION "${HUARAY_BIN_DIR}/MVSDKmd.dll")
        endif ()
    endif ()
endif ()

if (SZX_CORE_HAS_HUARAY)
    message(STATUS "Huaray SDK detected: ${HUARAY_ROOT}")
else ()
    message(STATUS "Huaray SDK not detected; Huaray adapters will not be built. Set -DHUARAY_ROOT=...")
endif ()

# =====================================================================
# Optional dependency: libmodbus (required for plc_core)
# =====================================================================
set(SZX_CORE_HAS_LIBMODBUS OFF)
if (WIN32 AND EXISTS "${LIBMODBUS_ROOT}/include" AND EXISTS "${LIBMODBUS_ROOT}/lib/libmodbus.lib")
    set(SZX_CORE_HAS_LIBMODBUS ON)
    add_library(libmodbus::libmodbus SHARED IMPORTED GLOBAL)
    set_target_properties(libmodbus::libmodbus PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIBMODBUS_ROOT}/include"
    )
    set_target_properties(libmodbus::libmodbus PROPERTIES
        IMPORTED_IMPLIB_RELEASE   "${LIBMODBUS_ROOT}/lib/libmodbus.lib"
        IMPORTED_LOCATION_RELEASE "${LIBMODBUS_ROOT}/bin/libmodbus.dll"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL     Release
    )
    if (EXISTS "${LIBMODBUS_ROOT}/lib/libmodbusd.lib")
        set_target_properties(libmodbus::libmodbus PROPERTIES
            IMPORTED_IMPLIB_DEBUG   "${LIBMODBUS_ROOT}/lib/libmodbusd.lib"
            IMPORTED_LOCATION_DEBUG "${LIBMODBUS_ROOT}/bin/libmodbusd.dll"
        )
    endif ()
endif ()

if (SZX_CORE_HAS_LIBMODBUS)
    message(STATUS "libmodbus detected: ${LIBMODBUS_ROOT}")
else ()
    message(STATUS "libmodbus not detected; plc_core will NOT be built. Set -DLIBMODBUS_ROOT=...")
endif ()

# =====================================================================
# Target: common_support
# =====================================================================
add_library(common_support
    src/common/log/log_manager.cpp
)
add_library(szx_core::common_support ALIAS common_support)

target_include_directories(common_support
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (TARGET spdlog::spdlog)
    target_link_libraries(common_support PUBLIC spdlog::spdlog)
else ()
    target_link_libraries(common_support PUBLIC spdlog::spdlog_header_only)
endif ()

# =====================================================================
# Target: camera_core
# =====================================================================
set(CAMERA_CORE_SOURCES
    src/camera/core/camera_device.cpp
    src/camera/core/camera_manager.cpp
    src/camera/core/camera_pipeline.cpp
    src/camera/core/camera_sdk_adapter.cpp
    src/camera/core/camera_service.cpp
)

if (SZX_CORE_HAS_OPENCV)
    list(APPEND CAMERA_CORE_SOURCES
        src/camera/filecamera/camera_fileimage_sdk_adapter.cpp
    )
endif ()

if (SZX_CORE_HAS_IKAP)
    list(APPEND CAMERA_CORE_SOURCES
        src/camera/ikap/camera_ikapsdk_adapter.cpp
        src/camera/ikap/ikap_runtime.cpp
    )
endif ()

if (SZX_CORE_HAS_HUARAY)
    list(APPEND CAMERA_CORE_SOURCES
        src/camera/huaray/camera_huaraysdk_adapter.cpp
        src/camera/huaray/camera_huaray_usb_adapter.cpp
    )
endif ()

add_library(camera_core ${CAMERA_CORE_SOURCES})
add_library(szx_core::camera_core ALIAS camera_core)

target_include_directories(camera_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(camera_core PUBLIC common_support)

if (WIN32)
    target_link_libraries(camera_core PUBLIC ws2_32)
endif ()

if (SZX_CORE_HAS_IKAP)
    target_link_libraries(camera_core PUBLIC IKap::IKapC IKap::IKapBoard)
    target_compile_definitions(camera_core PUBLIC SZX_CORE_HAS_IKAP=1)
endif ()

if (SZX_CORE_HAS_HUARAY)
    target_link_libraries(camera_core PUBLIC Huaray::IMVFG)
    target_compile_definitions(camera_core PUBLIC SZX_CORE_HAS_HUARAY=1)
endif ()

if (SZX_CORE_HAS_OPENCV)
    target_link_libraries(camera_core PUBLIC ${OpenCV_LIBS})
    target_include_directories(camera_core PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(camera_core PUBLIC SZX_CORE_HAS_OPENCV=1)
endif ()

if (BUILD_SHARED_LIBS)
    target_compile_definitions(camera_core
        PRIVATE CAMERA_CORE_EXPORTS
        PUBLIC  CAMERA_CORE_SHARED
    )
endif ()

# =====================================================================
# Target: plc_core (only if libmodbus available)
# =====================================================================
if (SZX_CORE_HAS_LIBMODBUS)
    add_library(plc_core
        src/plc/plccontrol.cpp
    )
    add_library(szx_core::plc_core ALIAS plc_core)

    target_include_directories(plc_core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(plc_core
        PUBLIC
            common_support
            libmodbus::libmodbus
    )

    if (BUILD_SHARED_LIBS)
        target_compile_definitions(plc_core
            PRIVATE PLC_CORE_EXPORTS
            PUBLIC  PLC_CORE_SHARED
        )
    endif ()
endif ()

# =====================================================================
# Warnings
# =====================================================================
if (MSVC)
    target_compile_options(common_support PRIVATE /W4 /utf-8)
    target_compile_options(camera_core    PRIVATE /W4 /utf-8)
    if (TARGET plc_core)
        target_compile_options(plc_core   PRIVATE /W4 /utf-8)
    endif ()
else ()
    target_compile_options(common_support PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(camera_core    PRIVATE -Wall -Wextra -Wpedantic)
    if (TARGET plc_core)
        target_compile_options(plc_core   PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endif ()

# =====================================================================
# Examples / Tests
# =====================================================================
if (SZX_CORE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (SZX_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# =====================================================================
# Install / Export (opt-in)
#
# Install rules are off by default because the simplest integration is
# add_subdirectory(). Enable -DSZX_CORE_INSTALL=ON if you need to
# install the libraries and use them via find_package(szx_core).
#
# Note: when spdlog comes from FetchContent (the default fallback path)
# install will fail with "spdlog not in any export set". Either:
#   1. Install spdlog separately first, then build szx_core with
#      -DSZX_CORE_FETCH_SPDLOG=OFF so find_package(spdlog) picks it up.
#   2. Vendor spdlog into third_party/spdlog/ before configuring.
# =====================================================================
if (SZX_CORE_INSTALL)
    set(SZX_CORE_INSTALL_TARGETS common_support camera_core)
    if (TARGET plc_core)
        list(APPEND SZX_CORE_INSTALL_TARGETS plc_core)
    endif ()

    install(TARGETS ${SZX_CORE_INSTALL_TARGETS}
        EXPORT  szx_coreTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT szx_coreTargets
        FILE        szx_coreTargets.cmake
        NAMESPACE   szx_core::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/szx_core
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/szx_coreConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/szx_coreConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/szx_core
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/szx_coreConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/szx_coreConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/szx_coreConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/szx_core
    )
endif ()

# =====================================================================
# Summary
# =====================================================================
message(STATUS "")
message(STATUS "================ szx_core build summary ================")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  common_support:    yes")
message(STATUS "  camera_core:       yes")
message(STATUS "    + FileImage adapter: ${SZX_CORE_HAS_OPENCV} (needs OpenCV)")
message(STATUS "    + IKap adapter:      ${SZX_CORE_HAS_IKAP}")
message(STATUS "    + Huaray adapter:    ${SZX_CORE_HAS_HUARAY}")
message(STATUS "  plc_core:          ${SZX_CORE_HAS_LIBMODBUS}")
message(STATUS "  examples:          ${SZX_CORE_BUILD_EXAMPLES}")
message(STATUS "  tests:             ${SZX_CORE_BUILD_TESTS}")
message(STATUS "========================================================")
message(STATUS "")
