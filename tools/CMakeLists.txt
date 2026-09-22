if(SZX_COMPONENTS_BUILD_RBAC)
    add_subdirectory(szx_admin)
endif()

add_subdirectory(license-issuer)
add_subdirectory(license-demo)

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS)
    add_subdirectory(license-issuer-gui)
endif()

if(SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP)
    if(NOT SZX_COMPONENTS_BUILD_UI_TOOLKITS)
        message(FATAL_ERROR "SZX_COMPONENTS_BUILD_UI_TOOLKITS_APP requires SZX_COMPONENTS_BUILD_UI_TOOLKITS=ON")
    endif()
    add_subdirectory(UIToolkitsApp)
endif()
