set(SZX_DMC_SOURCES
    src/DmcController.cc
    src/SimulatedDmcController.cc)

set(SZX_DMC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/LTDMC" CACHE PATH
    "LeiSai LTDMC SDK root")
set(SZX_DMC_HEADER "${SZX_DMC_ROOT}/LTDMC.h")
set(SZX_DMC_IMPLIB "${SZX_DMC_ROOT}/LTDMC.lib")
set(SZX_DMC_RUNTIME "${SZX_DMC_ROOT}/LTDMC.dll")
set(SZX_DMC_HAS_LTDMC FALSE)
if(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8
   AND EXISTS "${SZX_DMC_HEADER}" AND EXISTS "${SZX_DMC_IMPLIB}")
    set(SZX_DMC_HAS_LTDMC TRUE)
    message(STATUS "DMC SDK: ${SZX_DMC_ROOT}")
else()
    message(STATUS "DMC SDK: unavailable; hardware adapter disabled")
endif()
if(SZX_DMC_HAS_LTDMC)
    list(APPEND SZX_DMC_SOURCES
        src/LTDMC/LTDMCE3032Controller.cc src/LTDMC/LTDMCE3032Controller.hpp
    )
endif()

add_library(szx_dmc SHARED ${SZX_DMC_SOURCES})
add_library(szx::dmc ALIAS szx_dmc)
set_target_properties(szx_dmc PROPERTIES EXPORT_NAME dmc)
target_compile_definitions(szx_dmc PRIVATE SZX_DMC_EXPORTS)
target_include_directories(szx_dmc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(szx_dmc PRIVATE szx::common)
if(SZX_DMC_HAS_LTDMC)
    target_include_directories(szx_dmc PRIVATE "${SZX_DMC_ROOT}")
    target_link_libraries(szx_dmc PRIVATE "${SZX_DMC_IMPLIB}")
    target_compile_definitions(szx_dmc PRIVATE SZX_DMC_HAS_LTDMC=1)
endif()
target_compile_features(szx_dmc PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_dmc)

add_library(szx_dmc_static STATIC ${SZX_DMC_SOURCES})
add_library(szx::dmc_static ALIAS szx_dmc_static)
set_target_properties(szx_dmc_static PROPERTIES EXPORT_NAME dmc_static)
target_compile_definitions(szx_dmc_static PUBLIC SZX_DMC_STATIC)
target_include_directories(szx_dmc_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(szx_dmc_static PUBLIC szx::common)
if(SZX_DMC_HAS_LTDMC)
    target_include_directories(szx_dmc_static PRIVATE "${SZX_DMC_ROOT}")
    target_link_libraries(szx_dmc_static PUBLIC "${SZX_DMC_IMPLIB}")
    target_compile_definitions(szx_dmc_static PRIVATE SZX_DMC_HAS_LTDMC=1)
endif()
target_compile_features(szx_dmc_static PUBLIC cxx_std_20)
szx_apply_default_compile_policy(szx_dmc_static)
set_target_properties(szx_dmc_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (WIN32)
    target_compile_definitions(szx_dmc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(szx_dmc_static PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (COMMAND szx_copy_runtime_dlls)
    szx_copy_runtime_dlls(szx_dmc)
endif()
if(SZX_DMC_HAS_LTDMC AND EXISTS "${SZX_DMC_RUNTIME}")
    add_custom_command(TARGET szx_dmc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SZX_DMC_RUNTIME}" "$<TARGET_FILE_DIR:szx_dmc>")
    if(SZX_COMPONENTS_INSTALL)
        install(FILES "${SZX_DMC_RUNTIME}" DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
    endif()
endif()

if(COMMAND szx_find_python)
    # Use the Python interpreter selected by the top-level project.  The
    # helper prefers <top-level>/.venv and keeps pybind11 and the extension
    # on the same Python installation.
    szx_find_python()
endif()

if(SZX_PYTHON)
    set(Python_EXECUTABLE "${SZX_PYTHON}" CACHE FILEPATH "Python interpreter used by DMC pybind11" FORCE)
    set(Python3_EXECUTABLE "${SZX_PYTHON}" CACHE FILEPATH "Python 3 interpreter used by DMC pybind11" FORCE)
    set(PYTHON_EXECUTABLE "${SZX_PYTHON}" CACHE FILEPATH "Python interpreter used by DMC pybind11" FORCE)
    execute_process(
        COMMAND "${SZX_PYTHON}" -c "import pybind11; print(pybind11.get_cmake_dir())"
        RESULT_VARIABLE _szx_pybind11_result
        OUTPUT_VARIABLE _szx_pybind11_cmake_dir
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_szx_pybind11_result EQUAL 0 AND EXISTS "${_szx_pybind11_cmake_dir}")
        list(PREPEND CMAKE_PREFIX_PATH "${_szx_pybind11_cmake_dir}")
        message(STATUS "DMC pybind11: ${_szx_pybind11_cmake_dir}")
    endif()
endif()

find_package(pybind11 CONFIG QUIET)
if(pybind11_FOUND)
    pybind11_add_module(szx_dmc_py MODULE python/python_bindings.cc)
    target_link_libraries(szx_dmc_py PRIVATE szx::dmc_static)
    target_compile_features(szx_dmc_py PRIVATE cxx_std_20)

    set(SZX_DMC_PYI_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/python/szx_dmc_py.pyi.in")
    set(SZX_DMC_PYI_BUILD "${CMAKE_CURRENT_BINARY_DIR}/szx_dmc_py.pyi")
    add_custom_command(
        OUTPUT "${SZX_DMC_PYI_BUILD}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SZX_DMC_PYI_TEMPLATE}" "${SZX_DMC_PYI_BUILD}"
        DEPENDS "${SZX_DMC_PYI_TEMPLATE}"
        COMMENT "Generating szx_dmc_py.pyi")
    add_custom_target(szx_dmc_pyi DEPENDS "${SZX_DMC_PYI_BUILD}")
    add_dependencies(szx_dmc_py szx_dmc_pyi)
    add_custom_command(TARGET szx_dmc_py POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SZX_DMC_PYI_BUILD}"
                "$<TARGET_FILE_DIR:szx_dmc_py>/szx_dmc_py.pyi")

    if(COMMAND szx_copy_runtime_dlls)
        szx_copy_runtime_dlls(szx_dmc_py)
    endif()
    if(SZX_DMC_HAS_LTDMC AND EXISTS "${SZX_DMC_RUNTIME}")
        add_custom_command(TARGET szx_dmc_py POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SZX_DMC_RUNTIME}" "$<TARGET_FILE_DIR:szx_dmc_py>")
    endif()
    if(SZX_COMPONENTS_INSTALL)
        install(FILES "${SZX_DMC_PYI_BUILD}" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()
