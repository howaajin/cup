#include "cup/cup.h"
#include "cup/cup.private.h"

ENTRY(build_c_compile_cmd_llvm)
{
    Node* src = SRC("{dir}/c_compile_cmd_llvm.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/depfile.c"));
}

ENTRY(build_c_compile_cmd_gcc)
{
    Node* src = SRC("{dir}/c_compile_cmd_gcc.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_c_compile_cmd_msvc)
{
    Node* src = SRC("{dir}/c_compile_cmd_msvc.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}
ENTRY(build_c_compile_cmd_zigcc)
{
    Node* src = SRC("{dir}/c_compile_cmd_zigcc.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_cpp_module)
{
    Node* src = SRC("{dir}/cpp_module.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/c_compile_cmd.c"));
}

ENTRY(build_c_compile_cmd)
{
    Node* src = SRC("{dir}/c_compile_cmd.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/cpp_module.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/cup.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_compile_cmd_gcc.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_compile_cmd_llvm.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_compile_cmd_zigcc.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_compile_cmd_msvc.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/scan_deps_cmd.c"));
}

ENTRY(set_var_platform_c_toolchain, PRIORITY_BEFORE_DEFAULT)
{
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    set_var("platform_c_toolchain_c", fmt("{dir}/c_toolchain_windows.c"));
#elif CURRENT_PLATFORM == PLATFORM_LINUX || CURRENT_PLATFORM == PLATFORM_MACOS
    set_var("platform_c_toolchain_c", fmt("{dir}/c_toolchain_linux_mac.c"));
#endif
}

ENTRY(build_platform_c_toolchain)
{
    Node* src = SRC("{platform_c_toolchain_c}");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
    obj_add_link_obj_from_src(obj, SRC("{platform_directory_c}"));
    obj_add_link_obj_from_src(obj, SRC("src/core/{platform}/dylib.c"));
}

ENTRY(build_c_toolchain)
{
    Node* src = SRC("{dir}/c_toolchain.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/gen_compile_commands.c"));
    obj_add_link_obj_from_src(obj, SRC("{platform_c_toolchain_c}"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/link_cmd.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/c_compile_cmd.c"));
}

ENTRY(build_link_cmd)
{
    Node* src = SRC("{dir}/link_cmd.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_ar_cmd)
{
    Node* src = SRC("{dir}/ar_cmd.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_gen_compile_commands)
{
    Node* src = SRC("{dir}/gen_compile_commands.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/entry.c"));
}

ENTRY(build_scan_test)
{
    Node* src = SRC("{dir}/scan_test.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/test_finder.c"));
}

ENTRY(build_scan_deps_cmd)
{
    Node* src = SRC("{dir}/scan_deps_cmd.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/core/json.c"));
}