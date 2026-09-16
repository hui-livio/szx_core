cmake_minimum_required(VERSION 3.21)
project(NetworkPackageConsumer LANGUAGES CXX)
find_package(SZXComponents 0.1 CONFIG REQUIRED COMPONENTS Network)
get_target_property(_network_dependencies szx::network INTERFACE_LINK_LIBRARIES)
if(NOT _network_dependencies STREQUAL "szx::common")
    message(FATAL_ERROR "Network exported unexpected dependencies: ${_network_dependencies}")
endif()
add_executable(network_package_consumer main.cc)
target_link_libraries(network_package_consumer PRIVATE szx::network)
add_custom_command(TARGET network_package_consumer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:szx::network> $<TARGET_FILE_DIR:network_package_consumer>)
enable_testing()
add_test(NAME network_package_consumer COMMAND network_package_consumer)
