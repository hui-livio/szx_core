find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Widgets)
find_package(Vulkan QUIET) # suppress Qt internal Vulkan not-found warning
qt_standard_project_setup(REQUIRES 6.8)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME "UIToolkitsApp")

qt_add_executable(${TARGET_NAME}
    main.cc
    src/DemoLauncher.h
    src/DemoLauncher.cc
    src/WidgetMainWindow.h
    src/WidgetMainWindow.cc
)

if(WIN32)
    target_sources(${TARGET_NAME} PRIVATE
        resource/app.rc
    )
    set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

qt_add_qml_module(${TARGET_NAME}
    URI UIToolkitsApp
    VERSION 1.0
    QML_FILES
    qml/Main.qml
    qml/DemoButtons.qml
    qml/DemoInputs.qml
    qml/DemoSelections.qml
    qml/DemoDisplay.qml
    qml/DemoFeedback.qml
    qml/DemoPickers.qml
    qml/DemoCharts.qml
    qml/DemoCalendar.qml
    qml/DemoTreeView.qml
    qml/DemoColors.qml
    qml/DemoTabBar.qml
    qml/DemoFootBar.qml
    qml/DemoMenu.qml
    qml/DemoImageViewer.qml
    qml/DemoMultiRectOverlay.qml
    qml/DemoDialogs.qml
    SOURCES
    src/model/TreeNode.h
    src/model/TreeModel.h
    src/model/TreeModel.cc
    src/model/NavTreeFilterModel.h
    src/model/NavTreeFilterModel.cc
    RESOURCES
    resource/app.ico
)
target_include_directories(${TARGET_NAME} PRIVATE
    src
    src/model
    src/widgetpages
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    UIToolkits
)


# --- 部署: windeployqt 自动复制 Qt DLL / plugins / qml 到构建目录 ---
szx_windeployqt(${TARGET_NAME}
    QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    NO_OPENGL_SW
)
