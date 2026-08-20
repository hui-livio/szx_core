include_guard(GLOBAL)

option(SZX_ENABLE_GIT_HOOKS "Install repository Git hooks during configure" OFF)

function(szx_setup_git_hooks)
    if(NOT SZX_ENABLE_GIT_HOOKS OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.git" OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.githooks")
        return()
    endif()
    execute_process(COMMAND git config core.hooksPath .githooks
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(WARNING "Unable to configure Git hooks")
    endif()
endfunction()

