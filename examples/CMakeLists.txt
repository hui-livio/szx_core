if (SZX_CORE_HAS_OPENCV)
    add_executable(camera_fileimage_demo camera_fileimage_demo.cpp)
    target_link_libraries(camera_fileimage_demo PRIVATE szx_core::camera_core)
endif ()

if (SZX_CORE_HAS_IKAP)
    add_executable(camera_ikap_demo camera_ikap_demo.cpp)
    target_link_libraries(camera_ikap_demo PRIVATE szx_core::camera_core)
endif ()

if (SZX_CORE_HAS_HUARAY)
    add_executable(camera_huaray_usb_demo camera_huaray_usb_demo.cpp)
    target_link_libraries(camera_huaray_usb_demo PRIVATE szx_core::camera_core)
endif ()
