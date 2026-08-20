get_property(_szx_opencv_found GLOBAL PROPERTY SZX_CAMERA_OPENCV_FOUND)
if(NOT _szx_opencv_found)
    message(STATUS "Skipping camera UT cases: OpenCV is unavailable")
    return()
endif()
get_property(_szx_opencv_libs GLOBAL PROPERTY SZX_CAMERA_OPENCV_LIBS)
get_property(_szx_opencv_includes GLOBAL PROPERTY SZX_CAMERA_OPENCV_INCLUDES)

szx_add_test(
    NAME szx_test_camera
      SOURCES CameraServiceCv_test.cc
      INCLUDES ${_szx_opencv_includes}
      LIBS szx::camera ${_szx_opencv_libs}
      COPY_RUNTIME
      LABELS unit component camera)
