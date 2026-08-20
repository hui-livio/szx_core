add_library(szx_common INTERFACE)
add_library(szx::common ALIAS szx_common)

file(GLOB_RECURSE SZX_COMMON_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
target_sources(szx_common INTERFACE ${SZX_COMMON_HEADERS})
target_include_directories(szx_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/common/ext>)
target_compile_features(szx_common INTERFACE cxx_std_20)

