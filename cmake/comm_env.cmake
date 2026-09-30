include_guard(GLOBAL)

function(szx_check_target_arch)
    if(WIN32 AND MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "SZXComponents requires a 64-bit MSVC toolchain")
    endif()
endfunction()

function(szx_find_python)
    cmake_parse_arguments(ARG "" "PATH" "" ${ARGN})
    if(ARG_PATH)
        set(SZX_PYTHON "${ARG_PATH}" CACHE FILEPATH "Python interpreter")
    else()
        # Prefer the virtual environment owned by the current top-level CMake
        # project.  This keeps configure-time scripts on the same Python
        # installation as the repository's dependencies.
        if(NOT SZX_PYTHON)
            if(WIN32)
                set(_szx_venv_python_names python.exe python)
                set(_szx_venv_python_path "${CMAKE_SOURCE_DIR}/.venv/Scripts")
            else()
                set(_szx_venv_python_names python3 python)
                set(_szx_venv_python_path "${CMAKE_SOURCE_DIR}/.venv/bin")
            endif()

            find_program(_szx_venv_python
                NAMES ${_szx_venv_python_names}
                PATHS "${_szx_venv_python_path}"
                NO_DEFAULT_PATH)
            if(_szx_venv_python)
                set(SZX_PYTHON "${_szx_venv_python}" CACHE FILEPATH "Python interpreter" FORCE)
            endif()
        endif()

        if(NOT SZX_PYTHON)
            find_program(SZX_PYTHON NAMES python3 python py)
        endif()
    endif()
    if(SZX_PYTHON)
        execute_process(COMMAND "${SZX_PYTHON}" --version OUTPUT_VARIABLE _version ERROR_VARIABLE _version OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "Python: ${SZX_PYTHON} (${_version})")
    else()
        message(FATAL_ERROR "Python not found — required to build SZX")
    endif()
endfunction()

