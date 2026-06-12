#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(PLC_CORE_SHARED)
#    if defined(PLC_CORE_EXPORTS)
#      define PLC_CORE_API __declspec(dllexport)
#    else
#      define PLC_CORE_API __declspec(dllimport)
#    endif
#  else
#    define PLC_CORE_API
#  endif
#else
#  define PLC_CORE_API
#endif
