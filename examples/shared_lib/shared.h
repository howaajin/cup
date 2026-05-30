#pragma once

#if defined(_WIN32) && defined(BUILDING_DLL)
#  define SHARED_API __declspec(dllexport)
#elif defined(_WIN32)
#  define SHARED_API __declspec(dllimport)
#else
#  define SHARED_API
#endif

SHARED_API int shared_func(int value);
