#include "core/allocator.h"
#include "core/array.h"
#include "core/dylib.h"
#include "core/hash.h"
#include "core/os.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/embedded_file.h"
#include "cup/entry.h"
#include "cup/node.h"
#include "cup/var.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern uint8_t bin2c_cup_h[];
extern size_t bin2c_cup_h_size;
extern uint8_t bin2c_cup_def[];
extern size_t bin2c_cup_def_size;
extern Dylib* cup_dll;
extern void restart(void);

typedef int FnMain(int argc, char** argv);
void collect_build_scripts(char const* directory, Allocator* allocator);
void add_build_script(char const* path);

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
ENTRY(gen_embedded_cup_def)
{
    static struct EmbeddedFile file = {
        .src = bin2c_cup_def,
        .size = &bin2c_cup_def_size,
        .path = "{out_dir}/cup.def",
        .type = FILE_TYPE_NORMAL,
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

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
static int gen_modified_def_cb(Node* node)
{
    char const* template_path = fmt("{out_dir}/cup.def");
    char const* modified_path = fmt("{out_dir}/cup_modified.def");

    char* content = os_read_all(allocator_temp(), template_path);
    if (!content)
    {
        fprintf(stderr, "error: cannot open %s\n", template_path);
        return EXIT_FAILURE;
    }

    char const* old_name = "NAME cup.exe";

    if (!string_starts_with(content, old_name))
    {
        fprintf(stderr, "error: unexpected format in %s\n", template_path);
        return EXIT_FAILURE;
    }

    char const* rest = content + strlen(old_name);
    char const* new_name = fmt("NAME {self_name}.exe");
    size_t new_len = strlen(new_name);
    size_t rest_len = strlen(rest);
    Allocator* temp = allocator_temp();
    char* result = string_new(temp, new_len + rest_len, NULL);
    memcpy(result, new_name, new_len);
    memcpy(result + new_len, rest, rest_len);

    if (!os_write_all(modified_path, result, new_len + rest_len))
    {
        fprintf(stderr, "error: cannot write %s\n", modified_path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif

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
    link->internal_flag = true;
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
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    {
        LinkCmd* lcmd = (LinkCmd*)link;

        // generate modified cup.def with correct NAME
        Node* def_template = FILE("{out_dir}/cup.def");
        Node* modified_def = FILE("{out_dir}/cup_modified.def");
        Node* gen_mod_cmd = CALLBACK_CMD(gen_modified_def_cb, NULL);
        cmd_set_source_location(gen_mod_cmd, __FILE__, __LINE__);
        cmd_add_input(gen_mod_cmd, def_template);
        cmd_add_output(gen_mod_cmd, modified_def);
        cmd_set_description(gen_mod_cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", modified_def));

        // generate import library from modified def
        Node* import_lib = get_or_add_file_with_type(fmt("{out_dir}/{self_name}" LIB_EXT), FILE_TYPE_LIB);
        make_implib_cmd_create(import_lib, modified_def, lcmd->toolchain, lcmd->arch, __FILE__, __LINE__);
        link_cmd_add_input(link, import_lib);

        if (lcmd->toolchain == TOOLCHAIN_TYPE_MSVC)
        {
            link_cmd_add_flag(link, "/export:main");
        }
        if (lcmd->toolchain == TOOLCHAIN_TYPE_LLVM)
        {
            link_cmd_add_flag(link, "-Wl,/export:main");
        }
    }
#endif
    cmd_set_after_execute_fn(link, after_self_built);
    allocator_destroy(allocator);
}

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
int bootstrap(void)
{
    entry_clean();
    Entry_register_gen_embedded_cup_h_interface();
    Entry_register_gen_build_c();
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    Entry_register_gen_embedded_cup_def();
#endif
    Entry_register_build_cup_dll();
    set_self_build_toolchain(get_default_toolchain());
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
