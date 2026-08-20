cmake_minimum_required(VERSION 3.21)

project(SZXComponents VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SZX_COMPONENTS_BUILD_TESTS "Build component smoke tests" ON)
option(SZX_COMPONENTS_INSTALL "Generate install tree and package config" ON)
option(SZX_COMPONENTS_BUILD_CAMERA "Build camera component" ON)
option(SZX_COMPONENTS_BUILD_PLC "Build PLC component when libmodbus is available" ON)
option(SZX_COMPONENTS_BUILD_IKAP "Build IKap adapter when SDK is available" OFF)

set(SZX_IKAP_ROOT "" CACHE PATH "IKap SDK root (optional)")
set(SZX_OPENCV_DIR "" CACHE PATH "OpenCV package directory (optional)")
set(SZX_LIBMODBUS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmodbus" CACHE PATH "libmodbus root")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/CMakeLists.txt")
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

if (SZX_COMPONENTS_BUILD_CAMERA)
    add_subdirectory(components/camera)
endif()

if (SZX_COMPONENTS_BUILD_PLC)
    add_subdirectory(components/plc)
endif()

if (SZX_COMPONENTS_INSTALL)
    set(SZX_COMPONENTS_INSTALL_TARGETS szx_common szx_logger szx_crypto)
    if (TARGET szx_camera)
        list(APPEND SZX_COMPONENTS_INSTALL_TARGETS szx_camera)
    endif()
    if (TARGET szx_plc)
        list(APPEND SZX_COMPONENTS_INSTALL_TARGETS szx_plc)
    endif()

    install(TARGETS ${SZX_COMPONENTS_INSTALL_TARGETS}
        EXPORT SZXComponentsTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(DIRECTORY components/common/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/szx/common
        FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY components/common/ext/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/szx/common/ext
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
    install(DIRECTORY components/crypto/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SzxCrypto
        FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY components/logger/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/szx/logger
        FILES_MATCHING PATTERN "Logger.h")
    if (TARGET szx_camera)
        install(DIRECTORY components/camera/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/szx/camera
            FILES_MATCHING PATTERN "*.h")
    endif()
    if (TARGET szx_plc)
        install(FILES components/plc/PlcControl.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/szx/plc)
    endif()

    install(EXPORT SZXComponentsTargets
        FILE SZXComponentsTargets.cmake
        NAMESPACE szx::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents)

    configure_package_config_file(
        cmake/SZXComponentsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents)
endif()

if (SZX_COMPONENTS_BUILD_TESTS)
    enable_testing()
    add_executable(szx_components_smoke_test tests/smoke.cpp)
    target_link_libraries(szx_components_smoke_test PRIVATE szx::common szx::crypto szx::logger)
    add_test(NAME szx_components_smoke_test COMMAND szx_components_smoke_test)
endif()

