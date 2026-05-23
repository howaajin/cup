#include "core/array.h"
#include "core/dylib.h"
#include "core/hash.h"
#include "core/os.h"
#include "core/platform.h"
#include "core/utilities.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/embedded_file.h"
#include "cup/entry.h"
#include "cup/var.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

extern uint8_t bin2c_cup_h[];
extern size_t bin2c_cup_h_size;
extern uint8_t bin2c_cup_lib[];
extern size_t bin2c_cup_lib_size;
extern Dylib* cup_dll;
extern void restart(void);

typedef int FnMain(int argc, char** argv);
void collect_build_scripts(char const* directory, Allocator* allocator);

#define ENTRY_REF(fn) \
    void fn(void);    \
    Entry fn##entry = {#fn, fn, __FILE__, __LINE__ + 1, 0};

ENTRY_REF(gen_embedded_cup_h_interface);
ENTRY(gen_embedded_cup_h_interface)
{
    static struct EmbeddedFile file = {
        .src = bin2c_cup_h,
        .size = &bin2c_cup_h_size,
        .path = "{out_dir}/cup/cup.h",
        .type = FILE_TYPE_NORMAL,
        .struct_bytes = sizeof(Node),
    };
    create_gen_embedded_file_cmd(&file);
}

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
ENTRY_REF(gen_embedded_cup_lib);
ENTRY(gen_embedded_cup_lib)
{
    static struct EmbeddedFile file = {
        .src = bin2c_cup_lib,
        .size = &bin2c_cup_lib_size,
        .path = "{out_dir}/cup" LIB_EXT,
        .type = FILE_TYPE_LIB,
        .struct_bytes = sizeof(Node),
    };
    create_gen_embedded_file_cmd(&file);
}
#endif

static void after_self_built(Node* cmd)
{
    cmd_after_execute(cmd);
    restart();
}

ENTRY_REF(build_cup_dll);
ENTRY(build_cup_dll)
{
    Allocator* allocator = allocator_create_chained();
    add_build_script("build.c");
    extern char** build_script_search_directories;
    for (size_t i = 0; i != array_size(build_script_search_directories); i++)
    {
        collect_build_scripts(build_script_search_directories[i], allocator);
    }
    char const* inc_cup_h = fmt("{out_dir}/cup");
    Node* cup_h = FILE("{out_dir}/cup/cup.h");
    Node* dll = DLL("{out_dir}/{self_name}");
    Node* link = LINK(dll);
    link_cmd_setup_self_build(link);
    link_cmd_set_arch(link, ARCH_X64);

    extern StringSet* build_scripts;
    for (uint32_t i = build_scripts->begin; i != build_scripts->end; i = hash_next(build_scripts, i))
    {
        Node* src = SRC(hash_key(build_scripts, i));
        Node* obj = OBJ(src);
        Node* cc = CC(src, obj);
        c_compile_cmd_add_self_build_options(cc);
        c_compile_cmd_add_include_directory(cc, inc_cup_h);
        node_add_dependency(cc, cup_h);
        link_cmd_add_input(link, obj);
        if (CURRENT_PLATFORM == PLATFORM_LINUX)
        {
            c_compile_cmd_add_flag(cc, "-fPIC");
        }
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        link_cmd_add_input(link, LIB("{out_dir}/cup"));
        LinkCmd* cmd = (LinkCmd*)link;
        if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
        {
            link_cmd_add_flag(link, "/export:main");
        }
        if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
        {
            link_cmd_add_flag(link, "-Wl,/export:main");
        }
    }
    cmd_set_after_execute_fn(link, after_self_built);
    allocator_destroy(allocator);
}

#undef ENTRY_REF

bool try_run_dll(int argc, char** argv, int* exit_code)
{
    char const* cup_dll_path = fmt("{out_dir}/{self_name}{dll_ext}");
    if (!os_file_exists(cup_dll_path))
    {
        return false;
    }
    cup_dll = dylib_load(cup_dll_path);
    if (cup_dll == NULL)
    {
        return false;
    }
    FnMain* main = (FnMain*)dylib_get_symbol(cup_dll, "main");
    if (main)
    {
        *exit_code = main(argc, argv);
        return true;
    }
    return true;
}

extern bool b_dll_mode;
extern ToolchainType self_build_toolchain;
extern Node** nodes;
extern void Entry_register_gen_build_c(void);
extern ToolchainType c_toolchain_select_toolchain_automatically();
extern int build_self(void);
extern char const* get_src_file_dir(char const* path, Allocator* allocator);
extern void invoke_entries_before_prepare(void);

#define ENTRY_REF(fn) fn##entry
int bootstrap(void)
{
    entry_clean();
    entry_push(ENTRY_REF(gen_embedded_cup_h_interface));
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    entry_push(ENTRY_REF(gen_embedded_cup_lib));
#endif
    entry_push(ENTRY_REF(build_cup_dll));
    set_self_build_toolchain(c_toolchain_select_toolchain_automatically());
    set_default_toolchain(self_build_toolchain);
    invoke_entries_before_prepare();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        node_ensure_prepared(nodes[i]);
    }
    Node* dll = DLL("{out_dir}/{self_name}");
    Node* cmd = dll->build_cmd;
    cmd_set_after_execute_fn(cmd, cmd_after_execute);
    return build_self();
}
#undef ENTRY_REF

extern int execute(void);

int main(int argc, char** argv)
{
    int exit_code;
    if (try_run_dll(argc, argv, &exit_code))
    {
        return exit_code;
    }
    return execute();
}

void init_mode(void)
{
    b_dll_mode = true;
    set_var("test_src_dir", get_var("out_dir"));

    extern bool b_bootstrap;
    if (b_bootstrap)
    {
        int exit_code = bootstrap();
        exit(exit_code);
    }
}
