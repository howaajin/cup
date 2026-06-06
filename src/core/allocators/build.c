#include "cup/cup.h"

static Node* get_build_allocator_ar_cmd()
{
    static Node* allocator_ar_cmd = NULL;
    if (allocator_ar_cmd == NULL)
    {
        allocator_ar_cmd = AR(LIB("{out_dir}/allocator"));
    }
    return allocator_ar_cmd;
}

ENTRY(build_c_allocator)
{
    Node* src = SRC("{dir}/c_allocator.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}

ENTRY(build_tiny_allocator)
{
    Node* src = SRC("{dir}/tiny_allocator.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}

ENTRY(build_arena_allocator)
{
    Node* src = SRC("{dir}/arena_allocator.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}

ENTRY(build_chained_allocator)
{
    Node* src = SRC("{dir}/chained_allocator.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}

ENTRY(build_virtual_allocator)
{
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    set_var("platform_allocator_c", fmt("{dir}/virtual_allocator_windows.c"));
#elif CURRENT_PLATFORM == PLATFORM_LINUX || CURRENT_PLATFORM == PLATFORM_MACOS
    set_var("platform_allocator_c", fmt("{dir}/virtual_allocator_linux_mac.c"));
#endif
    Node* src = SRC("{platform_allocator_c}");
    Node* obj = OBJ(src);
    CC(src, obj);
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}

ENTRY(build_temp_allocator)
{
    Node* src = SRC("{dir}/temp_allocator.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    if (get_default_toolchain() == TOOLCHAIN_TYPE_GCC && CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        obj_add_link_lib(obj, "pthread");
    }
    ar_cmd_add_input(get_build_allocator_ar_cmd(), obj);
}
