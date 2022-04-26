#pragma once

#if defined(_WIN32) && !defined(HAREFLOW_BUILDING_STATIC)
#    if defined(HAREFLOW_EXPORTS)
#        define HAREFLOW_EXPORT __declspec(dllexport)
#    else
#        define HAREFLOW_EXPORT __declspec(dllimport)
#    endif
#else
#    define HAREFLOW_EXPORT
#endif