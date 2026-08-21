SZX_FETCH_OpenCV()

szx_add_test(
    NAME test_modules_camera
    SOURCES
        CameraServiceCv_test.cc
    LIBS
        szx::camera
        ${OpenCV_LIBS}
)

target_include_directories(test_modules_camera PRIVATE ${OpenCV_INCLUDE_DIRS})
