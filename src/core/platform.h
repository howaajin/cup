#pragma once

#define COMPILER_CLANG 1
#define COMPILER_GCC 2
#define COMPILER_CL 3
#define COMPILER_TCC 4

#if defined(__clang__) && !defined(__CLANGD__)
#define CURRENT_COMPILER COMPILER_CLANG
#elif defined(__TINYC__)
#define CURRENT_COMPILER COMPILER_TCC
#elif defined(__GNUC__)
#define CURRENT_COMPILER COMPILER_GCC
#elif defined(_MSC_VER)
#define CURRENT_COMPILER COMPILER_CL
#endif

#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2
#define PLATFORM_MACOS 3
#define PLATFORM_ANDROID 4
#define PLATFORM_IOS 5

#ifdef _WIN32
#define CURRENT_PLATFORM PLATFORM_WINDOWS
#elif __APPLE__
#define CURRENT_PLATFORM PLATFORM_MACOS
#elif __linux__
#define CURRENT_PLATFORM PLATFORM_LINUX
#else
#error "Unknown platform"
#endif

#define ARCHITECTURE_X86 1
#define ARCHITECTURE_X64 2
#define ARCHITECTURE_ARM7 3
#define ARCHITECTURE_ARM8 4
#define ARCHITECTURE_UNKNOW 64

#ifdef _MSC_VER
#ifdef _M_X64
#define CURRENT_ARCHITECTURE ARCHITECTURE_X64
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X86
#else
#define CURRENT_ARCHITECTURE ARCHITECTURE_UNKNOW
#endif
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
#if defined(__i386__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X86
#elif defined(__x86_64__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X64
#elif defined(__aarch64__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_ARM8
#elif defined(__arm__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_ARM7
#else
#error "Unknown architecture for GCC/Clang"
#endif
#else
#error "Unknown compiler, unable to determine architecture"
#endif

#ifndef CONSTRUCTOR
#if CURRENT_COMPILER == COMPILER_CL
#define CONSTRUCTOR(name)                   \
    _Pragma("section(\".CRT$XCU\", read)"); \
    void name(void);                        \
    __declspec(allocate(".CRT$XCU")) void (*name##_ptr)(void) = name;
#define DESTRUCTOR(name)                    \
    _Pragma("section(\".CRT$XTU\", read)"); \
    void name(void);                        \
    __declspec(allocate(".CRT$XTU")) void (*name##_ptr)(void) = name;
#else
#define CONSTRUCTOR(name) __attribute__((constructor(101)))
#define DESTRUCTOR(name) __attribute__((destructor(101)))
#endif
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#else
#define DLL_EXPORT __attribute__((visibility("default")))
#define DLL_IMPORT
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define EXE_EXT ".exe"
#define DLL_EXT ".dll"
#define OBJ_EXT ".obj"
#define LIB_EXT ".lib"
#else
#define EXE_EXT ""
#define DLL_EXT ".so"
#define OBJ_EXT ".o"
#define LIB_EXT ".a"
#endif

static inline char const* get_platform_string(int type)
{
    switch (type)
    {
    case PLATFORM_WINDOWS: return "windows";
    case PLATFORM_LINUX: return "linux";
    case PLATFORM_MACOS: return "mac";
    case PLATFORM_ANDROID: return "android";
    case PLATFORM_IOS: return "ios";
    default: return "unknown platform";
    }
}

#if CURRENT_COMPILER == COMPILER_CLANG
#pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"
#endif
