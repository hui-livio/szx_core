add_library(szx_logger SHARED Logger.cc)
add_library(szx::logger ALIAS szx_logger)
target_include_directories(szx_logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/logger>)
target_link_libraries(szx_logger PRIVATE spdlog::spdlog)
target_compile_definitions(szx_logger PRIVATE SZX_LOG_EXPORTS)
target_compile_features(szx_logger PUBLIC cxx_std_20)

