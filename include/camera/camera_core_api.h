#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(CAMERA_CORE_SHARED)
#    if defined(CAMERA_CORE_EXPORTS)
#      define CAMERA_CORE_API __declspec(dllexport)
#    else
#      define CAMERA_CORE_API __declspec(dllimport)
#    endif
#  else
#    define CAMERA_CORE_API
#  endif
#else
#  define CAMERA_CORE_API
#endif
