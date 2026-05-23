#include "core/dylib.h"
#include "core/allocator.h"
#include "core/array.h"
#include "core/os.h"
#include "core/string.h"
#include "core/utilities.h"

#include <dlfcn.h>
#include <unistd.h>

typedef struct Dylib
{
    char const* name;
    void* handle;
} Dylib;

Dylib* dylib_load(char const* name)
{
    void* handle = dlopen(name, RTLD_LAZY);
    if (!handle)
    {
        return NULL;
    }
    Allocator* ca = allocator_c();
    if (name == NULL)
    {
        name = os_get_current_exe_path(ca);
    }
    else
    {
        name = string_from_c_str(ca, name);
    }
    Dylib* lib = allocator_malloc(ca, sizeof(Dylib));
    lib->handle = handle;
    lib->name = name;
    return lib;
}

void dylib_unload(Dylib* lib)
{
    Allocator* ca = allocator_c();
    array_free(allocator_c(), lib->name);
    dlclose(lib->handle);
    allocator_free(ca, lib);
}

void* dylib_get_symbol(Dylib* lib, char const* name)
{
    return dlsym(lib->handle, name);
}

void* dylib_get_image_base(Dylib* lib)
{
    return lib->handle;
}
