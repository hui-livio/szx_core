file(GLOB SZX_CAMERA_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
add_library(camera SHARED ${SZX_CAMERA_CORE_SOURCES})
add_library(szx::camera ALIAS camera)
target_include_directories(camera PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/camera>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(camera PRIVATE szx::common szx::logger)
target_compile_definitions(camera PRIVATE AOI_CAMERA_EXPORTS)
if (WIN32)
    target_compile_definitions(camera PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_compile_features(camera PUBLIC cxx_std_20)
if (WIN32)
    target_link_libraries(camera PRIVATE ws2_32)
endif()

if (SZX_OPENCV_DIR)
    set(OpenCV_DIR "${SZX_OPENCV_DIR}")
endif()
find_package(OpenCV CONFIG QUIET COMPONENTS core imgcodecs imgproc)
if (OpenCV_FOUND)
    target_sources(camera PRIVATE src/cv/camera_cv_adapter.cc)
    target_link_libraries(camera PRIVATE ${OpenCV_LIBS})
    target_include_directories(camera PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(camera PRIVATE SZX_HAS_OPENCV=1)
endif()

if (SZX_COMPONENTS_BUILD_IKAP AND WIN32 AND EXISTS "${SZX_IKAP_ROOT}/Include/IKapC.h")
    target_sources(camera PRIVATE src/ikap/camera_ikapsdk_adapter.cc src/ikap/ikap_runtime.cc)
    target_include_directories(camera PRIVATE "${SZX_IKAP_ROOT}/Include" src/ikap)
    target_compile_definitions(camera PRIVATE AOI_HAS_IKAP=1)
endif()
