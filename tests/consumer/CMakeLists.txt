cmake_minimum_required(VERSION 3.21)
project(SZXComponentsConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(SZXComponents 0.1 CONFIG REQUIRED
    COMPONENTS Common Crypto Logger Camera Plc)
add_executable(szx_components_consumer main.cpp)
target_link_libraries(szx_components_consumer PRIVATE
    szx::common szx::crypto szx::logger szx::camera szx::plc)
