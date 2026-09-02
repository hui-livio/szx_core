szx_find_opencv(QUIET)

if(OpenCV_FOUND)
    szx_add_test(
        NAME test_modules_camera
        SOURCES
        CameraServiceCv_test.cc
        LIBS
        szx::camera
        ${OpenCV_LIBS}
    )

    target_include_directories(test_modules_camera PRIVATE ${OpenCV_INCLUDE_DIRS})
else()
    message(STATUS "Skipping camera tests: OpenCV is unavailable")
endif()
