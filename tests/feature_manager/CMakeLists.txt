szx_add_test(
    NAME test_modules_feature_manager
    SOURCES
    FeatureManager_state_tests.cc
    FeatureManager_composite_tests.cc
    FeatureManager_local_file_tests.cc
    LIBS
    szx::feature_manager
)

target_include_directories(test_modules_feature_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/feature_manager/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/feature_manager/src
)
