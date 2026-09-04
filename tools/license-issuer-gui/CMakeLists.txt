find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Widgets)
qt_standard_project_setup(REQUIRES 6.8)

set(TARGET_NAME szx_license_issuer_gui)

qt_add_executable(${TARGET_NAME}
    main.cc
    src/IssuerGuiController.h
    src/IssuerGuiController.cc
    src/KeyFileHelper.h
    src/KeyFileHelper.cc
)

if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

qt_add_qml_module(${TARGET_NAME}
    URI SzxLicenseIssuer
    VERSION 1.0
    QML_FILES
    qml/Main.qml
    qml/PlaceholderPage.qml
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    UIToolkits
    szx::license
    szx::crypto
)

szx_apply_default_compile_policy(${TARGET_NAME})

szx_windeployqt(${TARGET_NAME}
    QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    NO_OPENGL_SW
)
