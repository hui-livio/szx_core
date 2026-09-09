add_subdirectory(common)
add_subdirectory(logger)
add_subdirectory(crypto)
add_subdirectory(crypto_service)
add_subdirectory(license)

if(SZX_COMPONENTS_BUILD_COMPRESSION)
    add_subdirectory(compression)
endif()

if(SZX_COMPONENTS_BUILD_FEATURE_MANAGER)
    add_subdirectory(feature_manager)
endif()

if(SZX_COMPONENTS_BUILD_CAMERA)
    add_subdirectory(camera)
endif()

if(SZX_COMPONENTS_BUILD_PLC)
    add_subdirectory(plc)
endif()

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS)
    add_subdirectory(UIToolkits)
endif()
