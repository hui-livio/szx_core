# Opt-in: no OpenCV dependency is added to admin or unrelated components.
szx_find_opencv(COMPONENTS core imgcodecs imgproc)
add_library(szx_web_image_core STATIC src/ImageService.cc src/ImageCatalog.cc)
target_include_directories(szx_web_image_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(szx_web_image_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(szx_web_image_core PUBLIC szx::network szx::common ${OpenCV_LIBS})
target_compile_features(szx_web_image_core PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_web_image_core)

add_executable(szx_web_image_test src/main.cc)
target_link_libraries(szx_web_image_test PRIVATE szx_web_image_core)
szx_apply_default_compile_policy(szx_web_image_test)
szx_copy_runtime_dlls(szx_web_image_test)

# No Node/Vue/bundler step; copy assets on every build, including non-linking builds.
add_custom_target(szx_web_image_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:szx_web_image_test>/web-image"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:szx_web_image_test>/web-image/web"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:szx_web_image_test>/web-image/assets"
    VERBATIM)
add_dependencies(szx_web_image_test szx_web_image_assets)
