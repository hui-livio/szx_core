if(SZX_COMPONENTS_BUILD_RBAC)
    add_subdirectory(szx_admin)
endif()

add_subdirectory(license)

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP)
    if(NOT SZX_COMPONENTS_BUILD_UI_TOOLKITS)
        message(FATAL_ERROR "SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP requires SZX_COMPONENTS_BUILD_UI_TOOLKITS=ON")
    endif()
    add_subdirectory(UIToolkitsApp)
endif()

if(SZX_COMPONENTS_BUILD_WEB_IMAGE_TEST)
    if(NOT SZX_COMPONENTS_BUILD_NETWORK)
        message(FATAL_ERROR "SZX_COMPONENTS_BUILD_WEB_IMAGE_TEST requires SZX_COMPONENTS_BUILD_NETWORK=ON")
    endif()
    add_subdirectory(WebImageTest)
endif()
