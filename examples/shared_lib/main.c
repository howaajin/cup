#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define LIB_LOAD(name) LoadLibraryA(name)
#define LIB_GET_FUNC(lib, name) ((void (*)(void))GetProcAddress((lib), (name)))
#define LIB_FREE(lib) FreeLibrary((lib))
#define LIB_PATH "build/shared_lib.dll"
#else
#include <dlfcn.h>
#define LIB_LOAD(name) dlopen((name), RTLD_LAZY)
#define LIB_GET_FUNC(lib, name) dlsym((lib), (name))
#define LIB_FREE(lib) dlclose((lib))
#ifdef __APPLE__
#define LIB_PATH "build/shared_lib.dylib"
#else
#define LIB_PATH "build/shared_lib.so"
#endif
#endif

typedef int (*shared_func_fn)(int);

int main(void)
{
    void* lib = LIB_LOAD(LIB_PATH);
    if (!lib)
    {
        fprintf(stderr, "Failed to load shared library: %s\n", LIB_PATH);
        return 1;
    }
    shared_func_fn fn = (shared_func_fn)LIB_GET_FUNC(lib, "shared_func");
    if (!fn)
    {
        fprintf(stderr, "Failed to find symbol 'shared_func'\n");
        LIB_FREE(lib);
        return 1;
    }
    printf("shared_func(21) = %d\n", fn(21));
    LIB_FREE(lib);
    return 0;
}
