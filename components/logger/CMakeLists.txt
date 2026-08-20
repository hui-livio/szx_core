add_library(logger SHARED src/Logger.cc)
add_library(szx::logger ALIAS logger)
target_include_directories(logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/logger>)
target_link_libraries(logger PRIVATE spdlog::spdlog)
target_compile_definitions(logger PRIVATE SZX_LOG_EXPORTS)
target_compile_features(logger PUBLIC cxx_std_20)
