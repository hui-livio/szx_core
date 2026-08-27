find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 QmlIntegration Widgets)
find_package(Vulkan QUIET) # suppress Qt internal Vulkan not-found warning
qt_standard_project_setup(REQUIRES 6.8)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(UIToolkits SHARED)

# Export module output dir to parent so app deploy/install logic does not hardcode paths.
set(UITOOLKITS_QML_MODULE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/UIToolkits")
set_source_files_properties(qml/UTTooltip.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(UIToolkits
    URI UIToolkits
    VERSION 1.0
    OUTPUT_DIRECTORY "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}"
    QML_FILES
    qml/UTButton.qml
    qml/UTLineEdit.qml
    qml/UTLabel.qml
    qml/UTMarqueeLabel.qml
    qml/UTCheckBox.qml
    qml/UTCombobox.qml
    qml/UTSpinBox.qml
    qml/UTDoubleSpinBox.qml
    qml/UTSwitch.qml
    qml/UTCard.qml
    qml/UTSlider.qml
    qml/UTProgressBar.qml
    qml/UTImageAvatar.qml
    qml/UTToast.qml
    qml/UTMenu.qml
    qml/UTMenuBar.qml
    qml/UTTabBar.qml
    qml/UTTabButton.qml
    qml/UTDatePicker.qml
    qml/UTTimePicker.qml
    qml/UTDateTimePicker.qml
    qml/UTRoundButton.qml
    qml/UTTitleBar.qml
    qml/UTTitleBarButtonGroup.qml
    qml/UTFootBar.qml
    qml/UTImage.qml
    qml/UTSvgButton.qml
    qml/UTAppWindow.qml
    qml/UTScrollBar.qml
    qml/UTListView.qml
    qml/UTPieChart.qml
    qml/UTPieChartV2.qml
    qml/UTBarChart.qml
    qml/UTBarChartV2.qml
    qml/UTAreaChart.qml
    qml/UTAreaChartV2.qml
    qml/UTDataUtils.js
    qml/UTTreeView.qml
    qml/UTRangeSlider.qml
    qml/UTHsvColorWheel.qml
    qml/UTScrollView.qml
    qml/UTFlickable.qml
    qml/UTImageViewer.qml
    qml/UTMultiRectOverlay.qml
    qml/UTGroupBox.qml
    qml/UTKeyValueGrid.qml
    qml/UTTextArea.qml
    qml/UTSliderValue.qml
    qml/UTBadge.qml
    qml/UTStatCard.qml
    qml/UTHorizontalBarChart.qml
    qml/UTCalendar.qml
    qml/UTDialog.qml
    qml/UTMessageDialog.qml
    qml/UTLoading.qml
    qml/UTTooltip.qml
    SOURCES
    include/NativeWindow.h
    include/QmlPropertyMacros.h
    include/UIToolkitsExport.h
    include/UIToolkits/UTButton.h
    include/UIToolkits/UTLineEdit.h
    include/UIToolkits/UTLabel.h
    include/UIToolkits/UTCheckBox.h
    include/UIToolkits/UTSwitch.h
    include/UIToolkits/UTComboBox.h
    include/UIToolkits/UTSpinBox.h
    include/UIToolkits/UTSlider.h
    include/UIToolkits/UTProgressBar.h
    include/UIToolkits/UTCard.h
    include/UIToolkits/UTDialog.h
    include/UIToolkits/UTDoubleSpinBox.h
    include/UIToolkits/UTTextArea.h
    include/UIToolkits/UTRoundButton.h
    include/UIToolkits/UTSvgButton.h
    include/UIToolkits/UTTabBar.h
    include/UIToolkits/UTScrollBar.h
    include/UIToolkits/UTGroupBox.h
    include/UIToolkits/UTListView.h
    include/UIToolkits/UTTreeView.h
    include/UIToolkits/UTDatePicker.h
    include/UIToolkits/UTTimePicker.h
    include/UIToolkits/UTDateTimePicker.h
    include/UIToolkits/UTMenu.h
    include/UIToolkits/UTMenuBar.h
    include/UIToolkits/UTImage.h
    include/UIToolkits/UTWidgetTheme.h
    src/NativeWindow.cc
    src/UTTheme.h
    src/UTTheme.cc
    src/UTQmlLogger.h
    src/UTQmlLogger.cc
    src/widgets/UTButton.cc
    src/widgets/UTLineEdit.cc
    src/widgets/UTBasicWidgets.cc
    src/widgets/UTAdditionalWidgets.cc
    src/widgets/UTWidgetTheme.cc
    src/Charts/UTPieChartV2Renderer.h
    src/Charts/UTPieChartV2Renderer.cc
    src/Charts/UTBarChartV2Renderer.h
    src/Charts/UTBarChartV2Renderer.cc
    src/Charts/UTAreaChartV2Renderer.h
    src/Charts/UTAreaChartV2Renderer.cc
    RESOURCES
    fonts/HarmonyOS_Sans_SC_Regular.ttf
    fonts/HarmonyOS_Sans_SC_Medium.ttf
    fonts/HarmonyOS_Sans_SC_Bold.ttf
)



target_include_directories(UIToolkits
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src src/Charts
)
target_compile_definitions(UIToolkits PRIVATE UITOOLKITS_EXPORTS)

target_link_libraries(UIToolkits
    PUBLIC Qt6::Quick Qt6::QuickControls2 Qt6::Widgets
    PRIVATE Qt6::QmlIntegration
)

if(WIN32)
    target_link_libraries(UIToolkits PRIVATE dwmapi)
endif()

# Also output the plugin into the common bin directory so it's loadable at dev time.
set(_UITOOLKITS_PLUGIN_OUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/UIToolkits")
set_target_properties(UIToolkitsplugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_UITOOLKITS_PLUGIN_OUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${_UITOOLKITS_PLUGIN_OUT_DIR}"
)

# Copy qmldir and .qmltypes alongside the plugin so the QML engine can discover the module.
add_custom_command(TARGET UIToolkitsplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}/qmldir"
    "${_UITOOLKITS_PLUGIN_OUT_DIR}/qmldir"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}/UIToolkits.qmltypes"
    "${_UITOOLKITS_PLUGIN_OUT_DIR}/UIToolkits.qmltypes"
    COMMENT "Copying qmldir and .qmltypes to ${_UITOOLKITS_PLUGIN_OUT_DIR}"
)

# Make UIToolkits module output directory available for sibling subprojects.
set(UITOOLKITS_QML_MODULE_OUTPUT_DIR "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}" PARENT_SCOPE)

if(SZX_COMPONENTS_INSTALL)
    # Install runtime and plugin artifacts so cmake --install output can load module UIToolkits.
    install(TARGETS UIToolkits
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION lib
    )

    install(TARGETS UIToolkitsplugin
        RUNTIME DESTINATION bin/UIToolkits
        LIBRARY DESTINATION bin/UIToolkits
        ARCHIVE DESTINATION lib
    )

    install(FILES
        "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}/qmldir"
        "${UITOOLKITS_QML_MODULE_OUTPUT_DIR}/UIToolkits.qmltypes"
        DESTINATION bin/UIToolkits
    )
endif()
