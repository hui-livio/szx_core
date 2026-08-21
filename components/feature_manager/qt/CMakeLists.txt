find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_library(szx_feature_manager_qt STATIC
    include/QtFeatureManager.h
    src/QtFeatureManager.cc
)

target_include_directories(szx_feature_manager_qt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(szx_feature_manager_qt
    PUBLIC
        szx::feature_manager
        Qt6::Core
)

add_library(szx::feature_manager_qt ALIAS szx_feature_manager_qt)
