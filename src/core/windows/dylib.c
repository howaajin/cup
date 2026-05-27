#include "core/allocator.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdint.h>
#include <stdio.h>

typedef struct Dylib Dylib;

Dylib* dylib_load(char const* name)
{
    if (name == NULL)
    {
        return (Dylib*)GetModuleHandle(NULL);
    }
    else
    {
        Dylib* lib = (Dylib*)LoadLibraryA(name);
        if (lib == NULL)
        {
            char* messageBuffer = NULL;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&messageBuffer,
                0,
                NULL);
            if (messageBuffer)
            {
                fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, messageBuffer);
                fprintf(stderr, "%s\n", name);
                LocalFree(messageBuffer);
            }
        }
        return lib;
    }
}

void dylib_unload(Dylib* plugin)
{
    if ((HANDLE)plugin != GetModuleHandle(NULL))
    {
        FreeLibrary((HANDLE)plugin);
    }
}

void* dylib_get_symbol(Dylib* lib, char const* name)
{
    return (void*)GetProcAddress((HMODULE)lib, name);
}

void* dylib_get_image_base(Dylib* lib)
{
    return lib;
}
