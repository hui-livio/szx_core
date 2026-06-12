# Lightweight smoke tests. No googletest dependency to keep the dep tree minimal.
# Each test is a small main() returning 0 on success, non-zero on failure.
# Wired into CTest via add_test().

add_executable(test_camera_types  test_camera_types.cpp)
target_link_libraries(test_camera_types PRIVATE szx_core::camera_core)
add_test(NAME camera_types COMMAND test_camera_types)

add_executable(test_capture_pipeline test_capture_pipeline.cpp)
target_link_libraries(test_capture_pipeline PRIVATE szx_core::camera_core)
add_test(NAME capture_pipeline COMMAND test_capture_pipeline)
