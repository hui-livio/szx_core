include_guard(GLOBAL)

set(SZX_OUTPUT_BINDIR "bin" CACHE STRING "Build runtime output directory")
set(SZX_OUTPUT_LIBDIR "lib" CACHE STRING "Build library output directory")
set(SZX_OUTPUT_PDBDIR "pdb" CACHE STRING "Build PDB output directory")
set(SZX_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}" CACHE STRING "Install runtime directory")
set(SZX_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}" CACHE STRING "Install library directory")
set(SZX_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}" CACHE STRING "Install include directory")

function(_szx_output_path _out _path)
    if(IS_ABSOLUTE "${_path}")
        set(${_out} "${_path}" PARENT_SCOPE)
    else()
        set(${_out} "${CMAKE_BINARY_DIR}/${_path}" PARENT_SCOPE)
    endif()
endfunction()

function(szx_global_output_settings)
    cmake_parse_arguments(ARG "" "RUNTIME_DIR;LIBRARY_DIR;ARCHIVE_DIR;PDB_DIR" "" ${ARGN})
    if(NOT ARG_RUNTIME_DIR)
        set(ARG_RUNTIME_DIR "${SZX_OUTPUT_BINDIR}")
    endif()
    if(NOT ARG_LIBRARY_DIR)
        set(ARG_LIBRARY_DIR "${SZX_OUTPUT_LIBDIR}")
    endif()
    if(NOT ARG_ARCHIVE_DIR)
        set(ARG_ARCHIVE_DIR "${ARG_LIBRARY_DIR}")
    endif()
    _szx_output_path(_runtime "${ARG_RUNTIME_DIR}")
    _szx_output_path(_library "${ARG_LIBRARY_DIR}")
    _szx_output_path(_archive "${ARG_ARCHIVE_DIR}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_runtime}" PARENT_SCOPE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_library}" PARENT_SCOPE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_archive}" PARENT_SCOPE)
    foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_config}" _upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_upper} "${_runtime}/${_config}" PARENT_SCOPE)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_upper} "${_library}/${_config}" PARENT_SCOPE)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_upper} "${_archive}/${_config}" PARENT_SCOPE)
    endforeach()
endfunction()

function(szx_target_output_settings _target)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR;RUNTIME_DIR;LIBRARY_DIR;ARCHIVE_DIR;PDB_DIR" "" ${ARGN})
    if(NOT TARGET "${_target}")
        message(WARNING "szx_target_output_settings: target '${_target}' does not exist")
        return()
    endif()
    if(ARG_OUTPUT_DIR)
        set(ARG_RUNTIME_DIR "${ARG_OUTPUT_DIR}")
        set(ARG_LIBRARY_DIR "${ARG_OUTPUT_DIR}")
        set(ARG_ARCHIVE_DIR "${ARG_OUTPUT_DIR}")
    endif()
    foreach(_kind RUNTIME LIBRARY ARCHIVE)
        if(ARG_${_kind}_DIR)
            _szx_output_path(_dir "${ARG_${_kind}_DIR}")
            set_target_properties(${_target} PROPERTIES ${_kind}_OUTPUT_DIRECTORY "${_dir}")
            foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
                string(TOUPPER "${_config}" _upper)
                set_target_properties(${_target} PROPERTIES ${_kind}_OUTPUT_DIRECTORY_${_upper} "${_dir}/${_config}")
            endforeach()
        endif()
    endforeach()
    if(MSVC AND ARG_PDB_DIR)
        _szx_output_path(_pdb "${ARG_PDB_DIR}")
        set_target_properties(${_target} PROPERTIES PDB_OUTPUT_DIRECTORY "${_pdb}")
    endif()
endfunction()

function(szx_copy_runtime_dlls _target)
    cmake_parse_arguments(ARG "" "DESTINATION" "" ${ARGN})
    if(NOT TARGET "${_target}")
        message(WARNING "szx_copy_runtime_dlls: target '${_target}' does not exist")
        return()
    endif()
    get_target_property(_type "${_target}" TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    if(NOT WIN32)
        return()
    endif()
    # TARGET_RUNTIME_DLLS is empty for targets with no directly discoverable
    # runtime dependencies; avoid generating an invalid copy_if_different
    # command in that case.
    set(_runtime_targets)
    get_target_property(_linked_targets "${_target}" LINK_LIBRARIES)
    foreach(_linked IN LISTS _linked_targets)
        if(TARGET "${_linked}")
            get_target_property(_linked_type "${_linked}" TYPE)
            if(_linked_type STREQUAL "SHARED_LIBRARY" OR _linked_type STREQUAL "MODULE_LIBRARY")
                list(APPEND _runtime_targets "${_linked}")
            endif()
        endif()
    endforeach()
    if(NOT _runtime_targets)
        return()
    endif()
    if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION "$<TARGET_FILE_DIR:${_target}>")
    endif()
    add_custom_command(TARGET "${_target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${_target}> "${ARG_DESTINATION}"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs for ${_target}")
endfunction()

function(szx_install_target _target)
    cmake_parse_arguments(ARG "EXCLUDE_FROM_ALL" "COMPONENT" "" ${ARGN})
    if(NOT TARGET "${_target}")
        message(WARNING "szx_install_target: target '${_target}' does not exist")
        return()
    endif()
    get_target_property(_type "${_target}" TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    set(_opts)
    if(ARG_EXCLUDE_FROM_ALL)
        list(APPEND _opts EXCLUDE_FROM_ALL)
    endif()
    set(_args TARGETS "${_target}"
        RUNTIME DESTINATION "${SZX_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${SZX_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${SZX_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${SZX_INSTALL_INCLUDEDIR}")
    if(ARG_COMPONENT)
        list(APPEND _args COMPONENT "${ARG_COMPONENT}")
    endif()
    install(${_opts} ${_args})
endfunction()

function(szx_install_runtime_deps _target)
    if(TARGET "${_target}" AND WIN32)
        install(FILES $<TARGET_RUNTIME_DLLS:${_target}> DESTINATION "${SZX_INSTALL_BINDIR}" OPTIONAL)
    endif()
endfunction()

function(szx_install_headers)
    cmake_parse_arguments(ARG "EXCLUDE_FROM_ALL" "HEADER_DIR;DESTINATION;COMPONENT" "FILE_PATTERN" ${ARGN})
    if(NOT ARG_HEADER_DIR)
        message(FATAL_ERROR "szx_install_headers: HEADER_DIR is required")
    endif()
    if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION ".")
    endif()
    if(ARG_DESTINATION STREQUAL ".")
        set(ARG_DESTINATION "")
    endif()
    if(NOT ARG_FILE_PATTERN)
        set(ARG_FILE_PATTERN "*.h;*.hpp")
    endif()
    set(_patterns)
    foreach(_pattern IN LISTS ARG_FILE_PATTERN)
        list(APPEND _patterns PATTERN "${_pattern}")
    endforeach()
    install(DIRECTORY "${ARG_HEADER_DIR}/" DESTINATION "${SZX_INSTALL_INCLUDEDIR}/${ARG_DESTINATION}"
        FILES_MATCHING ${_patterns})
endfunction()

function(szx_setup_packaging)
    cmake_parse_arguments(ARG "" "VENDOR;DESCRIPTION" "COMPONENTS" ${ARGN})
    if(NOT ARG_VENDOR)
        set(ARG_VENDOR "SZX")
    endif()
    if(NOT ARG_DESCRIPTION)
        set(ARG_DESCRIPTION "SZX Components")
    endif()
    if(NOT SZX_CPACK_GENERATOR)
        if(WIN32)
            set(SZX_CPACK_GENERATOR ZIP CACHE STRING "CPack generator" FORCE)
        elseif(APPLE)
            set(SZX_CPACK_GENERATOR DragNDrop CACHE STRING "CPack generator" FORCE)
        else()
            set(SZX_CPACK_GENERATOR TGZ CACHE STRING "CPack generator" FORCE)
        endif()
    endif()
    set(CPACK_GENERATOR "${SZX_CPACK_GENERATOR}")
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VENDOR "${ARG_VENDOR}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${ARG_DESCRIPTION}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    if(ARG_COMPONENTS)
        set(CPACK_COMPONENTS_ALL ${ARG_COMPONENTS})
    endif()
    include(CPack)
endfunction()

function(szx_install_components_package)
    if(NOT SZX_COMPONENTS_INSTALL)
        return()
    endif()

    set(_targets szx_common szx_logger szx_crypto)
    foreach(_crypto_target IN ITEMS szx_crypto_service szx_license)
        if(TARGET ${_crypto_target})
            list(APPEND _targets ${_crypto_target})
        endif()
    endforeach()
    if(TARGET szx_compression)
        list(APPEND _targets szx_compression)
    endif()
    if(TARGET szx_feature_manager)
        list(APPEND _targets szx_feature_manager)
    endif()
    if(TARGET szx_feature_manager_qt)
        list(APPEND _targets szx_feature_manager_qt)
    endif()
    if(TARGET szx_camera)
        list(APPEND _targets szx_camera)
    endif()
    if(TARGET szx_plc)
        list(APPEND _targets szx_plc)
    endif()

    set(SZX_COMPONENTS_HAS_FEATURE_MANAGER_QT FALSE)
    if(TARGET szx_feature_manager_qt)
        set(SZX_COMPONENTS_HAS_FEATURE_MANAGER_QT TRUE)
    endif()

    install(TARGETS ${_targets}
        EXPORT SZXComponentsTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(TARGETS spdlog spdlog_header_only
        EXPORT SZXComponentsTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/spdlog/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.h" PATTERN "*.inl")

    szx_install_headers(common HEADER_DIR "${CMAKE_SOURCE_DIR}/components/common"
        DESTINATION szx/common FILE_PATTERN "*.hpp")
    szx_install_headers(common HEADER_DIR "${CMAKE_SOURCE_DIR}/components/common/ext"
        DESTINATION szx/common/ext FILE_PATTERN "*.h" "*.hpp")
    szx_install_headers(crypto HEADER_DIR "${CMAKE_SOURCE_DIR}/components/crypto/include"
        DESTINATION . FILE_PATTERN "*.h")
    if(TARGET szx_crypto_service)
        szx_install_headers(crypto_service HEADER_DIR "${CMAKE_SOURCE_DIR}/components/crypto_service/include"
            DESTINATION . FILE_PATTERN "*.h")
    endif()
    if(TARGET szx_license)
        szx_install_headers(license HEADER_DIR "${CMAKE_SOURCE_DIR}/components/license/include"
            DESTINATION . FILE_PATTERN "*.h")
    endif()
    if(TARGET szx_compression)
        szx_install_headers(compression HEADER_DIR "${CMAKE_SOURCE_DIR}/components/compression/include"
            DESTINATION . FILE_PATTERN "*.h")
    endif()
    install(FILES "${CMAKE_SOURCE_DIR}/components/logger/Logger.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/szx/logger")
    if(TARGET szx_feature_manager)
        szx_install_headers(feature_manager HEADER_DIR "${CMAKE_SOURCE_DIR}/components/feature_manager/include"
            DESTINATION szx/feature_manager FILE_PATTERN "*.h")
    endif()
    if(TARGET szx_feature_manager_qt)
        szx_install_headers(feature_manager_qt HEADER_DIR "${CMAKE_SOURCE_DIR}/components/feature_manager/qt/include"
            DESTINATION szx/feature_manager/qt FILE_PATTERN "*.h")
    endif()
    if(TARGET szx_camera)
        szx_install_headers(camera HEADER_DIR "${CMAKE_SOURCE_DIR}/components/camera/include"
            DESTINATION szx/camera FILE_PATTERN "*.h")
        szx_install_runtime_deps(szx_camera)
    endif()
    if(TARGET szx_plc)
        install(FILES "${CMAKE_SOURCE_DIR}/components/plc/PlcControl.h"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/szx/plc")
    endif()

    install(EXPORT SZXComponentsTargets
        FILE SZXComponentsTargets.cmake NAMESPACE szx::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents")
    configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/SZXComponentsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SZXComponentsConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SZXComponents")
    install(FILES
        "${CMAKE_SOURCE_DIR}/LICENCE.txt"
        "${CMAKE_SOURCE_DIR}/LICENCE.zh-CN.txt"
        DESTINATION licenses)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/spdlog/LICENSE")
        install(FILES "${CMAKE_SOURCE_DIR}/third_party/spdlog/LICENSE"
            DESTINATION licenses/spdlog)
    endif()
endfunction()
