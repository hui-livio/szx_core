set(_modbus_include "${SZX_LIBMODBUS_ROOT}/include")
set(_modbus_implib "${SZX_LIBMODBUS_ROOT}/lib/modbus.lib")
set(_modbus_dll "${SZX_LIBMODBUS_ROOT}/bin/modbus.dll")
if (WIN32 AND EXISTS "${_modbus_include}/modbus.h" AND EXISTS "${_modbus_implib}")
    add_library(libmodbus::modbus SHARED IMPORTED GLOBAL)
    set_target_properties(libmodbus::modbus PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_modbus_include}"
        IMPORTED_CONFIGURATIONS Release
        IMPORTED_IMPLIB_RELEASE "${_modbus_implib}"
        IMPORTED_LOCATION_RELEASE "${_modbus_dll}"
        MAP_IMPORTED_CONFIG_DEBUG Release
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release)
    add_library(plc SHARED src/PlcControl.cc src/PlcControlImpl.cc)
    target_compile_definitions(plc PRIVATE AOI_PLC_EXPORTS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(plc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/plc>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(plc PRIVATE szx::common szx::logger libmodbus::modbus)
    target_compile_features(plc PUBLIC cxx_std_20)
else()
    add_library(plc INTERFACE)
    target_include_directories(plc INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/plc>)
endif()
add_library(szx::plc ALIAS plc)
