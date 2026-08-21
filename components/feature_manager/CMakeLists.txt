add_library(szx_feature_manager STATIC
    include/FeatureAccess.h
    include/FeatureManager.h
    include/FeatureProvider.h
    include/FeatureValue.h
    src/CompositeFeatureProvider.h
    src/CompositeFeatureProvider.cc
    src/FeatureManager.cc
    src/FeatureValidation.h
    src/LocalFileFeatureProvider.h
    src/LocalFileFeatureProvider.cc)

add_library(szx::feature_manager ALIAS szx_feature_manager)
set_target_properties(szx_feature_manager PROPERTIES EXPORT_NAME feature_manager)
target_compile_features(szx_feature_manager PUBLIC cxx_std_20)
target_include_directories(szx_feature_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/szx/feature_manager>)
target_link_libraries(szx_feature_manager PUBLIC szx::common)
szx_apply_default_compile_policy(szx_feature_manager)

option(SZX_COMPONENTS_BUILD_FEATURE_MANAGER_QT "Build the Qt Feature Manager adapter" OFF)
if(SZX_COMPONENTS_BUILD_FEATURE_MANAGER_QT)
    add_subdirectory(qt)
endif()
