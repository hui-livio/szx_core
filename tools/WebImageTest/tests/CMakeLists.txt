szx_add_test(NAME test_web_image
    SOURCES ImageService_test.cc
    LIBS szx_web_image_core
    INCLUDES "${PROJECT_SOURCE_DIR}/tests/support"
    DEFINES "SZX_WEB_IMAGE_SOURCE=\"${CMAKE_CURRENT_SOURCE_DIR}/../web\""
    LABELS "web_image;network")
if(TARGET test_web_image AND WIN32)
    target_link_libraries(test_web_image PRIVATE ws2_32)
endif()
