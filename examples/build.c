#include "cup.h"

int main(int argc, char** argv)
{
    set_generate_vscode_files_enabled(false);
    if (get_default_optimization() != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    return execute();
}

ENTRY(build_hello)
{
    Node* src = SRC("hello/hello.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* exe = EXE("{out_dir}/hello");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj);
}

ENTRY(build_multi)
{
    Node* src_greet = SRC("multi/greet.c");
    Node* obj_greet = OBJ(src_greet);
    CC(src_greet, obj_greet);
    Node* src_main = SRC("multi/main.c");
    Node* obj_main = OBJ(src_main);
    CC(src_main, obj_main);
    Node* exe = EXE("{out_dir}/multi");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj_greet);
    link_cmd_add_input(link, obj_main);
}

ENTRY(build_mathlib)
{
    Node* src = SRC("static_lib/mathlib.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* lib = LIB("{out_dir}/mathlib");
    Node* ar = AR(lib);
    ar_cmd_add_input(ar, obj);
}

ENTRY(build_math_static)
{
    Node* src = SRC("static_lib/main.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* lib = LIB("{out_dir}/mathlib");
    Node* exe = EXE("{out_dir}/math_static");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj);
    link_cmd_add_input(link, lib);
}

ENTRY(build_shared_lib)
{
    Node* src = SRC("shared_lib/shared.c");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        c_compile_cmd_add_flag(cc, "-fPIC");
    }
    Node* dll = DLL("{out_dir}/shared_lib");
    Node* link = LINK(dll);
    link_cmd_add_input(link, obj);
}

ENTRY(build_shared_app)
{
    Node* src = SRC("shared_lib/main.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* exe = EXE("{out_dir}/shared_app");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj);
}

ENTRY(build_msgbox)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    Node* src;
    if (get_default_toolchain() == TOOLCHAIN_TYPE_MSVC)
    {
        src = SRC("asm_msgbox/msgbox.asm");
    }
    else
    {
        src = SRC("asm_msgbox/msgbox.s");
    }
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* exe = EXE("{out_dir}/msgbox");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj);
    link_cmd_add_lib(link, "user32");
    link_cmd_add_lib(link, "kernel32");
    link_cmd_set_entry(link, "WinMainCRTStartup");
    if (get_default_toolchain() == TOOLCHAIN_TYPE_MSVC)
    {
        link_cmd_add_flag(link, "/subsystem:windows");
        link_cmd_add_flag(link, "/nodefaultlib");
    }
    else
    {
        link_cmd_add_flag(link, "-nostartfiles");
        if (get_default_toolchain() == TOOLCHAIN_TYPE_LLVM)
        {
            link_cmd_add_flag(link, "-Wl,/subsystem:windows");
        }
        else
        {
            link_cmd_add_flag(link, "-mwindows");
        }
    }
}
