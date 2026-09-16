szx_add_libhv()
add_library(szx_network SHARED src/HttpServer.cc src/NetworkTypes.cc src/detail/HttpTransport.cc)
add_library(szx::network ALIAS szx_network)
set_target_properties(szx_network PROPERTIES EXPORT_NAME network
    VERSION 1.1.0 SOVERSION 1 CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_include_directories(szx_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# The existing C parser ships inside hv_static. Only the private adapter sees its header.
target_include_directories(szx_network PRIVATE "${SZX_LIBHV_ROOT}/http" "${SZX_LIBHV_ROOT}")
target_link_libraries(szx_network PUBLIC szx::common PRIVATE hv_static)
target_compile_definitions(szx_network PRIVATE SZX_NETWORK_EXPORTS
    SZX_NETWORK_HAS_TLS=$<BOOL:${SZX_NETWORK_ENABLE_TLS}>)
target_compile_features(szx_network PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_network)
