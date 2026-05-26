#include "core/os.h"
#include "core/string.h"
#include "core/utilities.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/cup.private.h"
#include "cup/entry.h"
#include "cup/node.h"
#include "cup/var.h"

#include "core/hash.h"

static void after_self_built(Node* cmd)
{
    cmd_after_execute(cmd);
    restart();
}

ENTRY(build_self_header_only)
{
    extern char const* cup_h_dir;
    extern char** build_script_search_directories;
    void collect_build_scripts(char const* directory, Allocator* allocator);
    Allocator* allocator = allocator_temp();
    add_build_script("build.c");
    for (size_t i = 0; i != array_size(build_script_search_directories); i++)
    {
        collect_build_scripts(build_script_search_directories[i], allocator);
    }
    Node* self = EXE("{self_name}");
    Node* cmd_link = LINK(self);
    link_cmd_setup_self_build(cmd_link);
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            Node* pdb = FILE("{out_dir}/cup.exe.pdb");
            link_cmd_set_pdb(cmd_link, pdb);
            if (default_toolchain == TOOLCHAIN_TYPE_GCC)
            {
                link_cmd_add_lib(cmd_link, "userenv");
                link_cmd_add_lib(cmd_link, "bcrypt");
            }
        }
        extern StringSet* build_scripts;
        for (uint32_t i = build_scripts->begin; i != build_scripts->end; i = hash_next(build_scripts, i))
        {
            Node* src = SRC(hash_key(build_scripts, i));
            Node* obj = OBJ(src);
            Node* cc = CC(src, obj);
            if (!string_equal(src->path, "build.c"))
            {
                c_compile_cmd_add_define(cc, "NO_BUILD_IMPLEMENTATION");
            }
            c_compile_cmd_add_self_build_options(cc);
            if (cup_h_dir)
            {
                c_compile_cmd_add_include_directory(cc, cup_h_dir);
            }
            if (default_toolchain == TOOLCHAIN_TYPE_MSVC)
            {
                c_compile_cmd_add_flag(cc, "/experimental:c11atomics");
            }
            if (CURRENT_PLATFORM == PLATFORM_LINUX)
            {
                c_compile_cmd_add_define(cc, "_GNU_SOURCE");
            }
            link_cmd_add_input(cmd_link, obj);
        }

        cmd_set_after_execute_fn(cmd_link, after_self_built);
    }
}

static void bootstrap_compile_link_make_cmdline_llvm_gcc_zig(Node* node, Node* out_exe)
{
    ToolchainType toolchain = default_toolchain;
    char const* compiler = (toolchain == TOOLCHAIN_TYPE_GCC ? "gcc" : (toolchain == TOOLCHAIN_TYPE_LLVM ? "clang" : "zig cc"));
    Node* cup_h = FILE("cup.h");
    cmd_add_option(node, NULL, compiler, OPTION_EXE);
    cmd_add_option(node, "-o ", out_exe->path, OPTION_OUTPUT);
    cmd_add_option(node, "-x c ", cup_h->path, OPTION_INPUT);
    cmd_add_option(node, "-D", "MAIN_ENTRY", OPTION_FLAG);
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        cmd_add_option(node, "-D", "_GNU_SOURCE", OPTION_FLAG);
        cmd_add_option(node, "-fms-extensions", NULL, OPTION_FLAG);
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (default_toolchain == TOOLCHAIN_TYPE_GCC)
        {
            cmd_add_option(node, "-l", "userenv", OPTION_FLAG);
            cmd_add_option(node, "-l", "bcrypt", OPTION_FLAG);
        }
    }
    cmd_add_input(node, cup_h);
    cmd_add_output(node, out_exe);
}

static void bootstrap_compile_link_make_cmdline_msvc(Node* node, Node* out_exe)
{
    extern Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);

    Node* env = msvc_get_env_node(default_toolchain, CURRENT_ARCHITECTURE);
    cmd_set_env(node, env);
    Node* cup_h = FILE("cup.h");
    Node* pdb = FILE("{out_dir}/{}.pdb", out_exe->path);
    node->ctx = pdb;
    cmd_add_option(node, NULL, "cl", OPTION_EXE);
    cmd_add_option(node, "/Fe:", out_exe->path, OPTION_OUTPUT);
    cmd_add_option(node, "/Tc ", cup_h->path, OPTION_INPUT);
    cmd_add_option(node, "/std:", "clatest", OPTION_FLAG);
    cmd_add_option(node, "/experimental:", "c11atomics", OPTION_FLAG);
    cmd_add_option(node, "/D", "MAIN_ENTRY", OPTION_BRIGHT_FLAG);
    cmd_add_option(node, "/Od", NULL, OPTION_FLAG);
    cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
    cmd_add_option(node, "/Z7", NULL, OPTION_FLAG);
    cmd_add_option(node, "/link", NULL, OPTION_FLAG);
    cmd_add_option(node, "/debug", NULL, OPTION_FLAG);
    cmd_add_option(node, "/incremental:", "no", OPTION_FLAG);
    cmd_add_option(node, "/noexp", NULL, OPTION_FLAG);
    cmd_add_option(node, "/noimplib", NULL, OPTION_FLAG);
    cmd_add_option(node, "/pdb:", pdb->path, OPTION_OUTPUT);
    cmd_add_input(node, cup_h);
    cmd_add_output(node, out_exe);
    cmd_add_output(node, pdb);
}

int build_self(void);
ToolchainType c_toolchain_select_toolchain_automatically();

static void bootstrap_compile_link_cmd_before_execute(Node* node)
{
    void c_toolchain_rename_to_old(char const* path);
    Node* output = node->extra_data;
    Node* pdb = node->ctx;
    if (!os_file_writable(output->path))
    {
        c_toolchain_rename_to_old(output->path);
    }
    if (pdb && !os_file_writable(pdb->path))
    {
        c_toolchain_rename_to_old(pdb->path);
    }
    cmd_before_execute(node);
}

int bootstrap(void)
{
    Node* self = EXE("{self_name}");
    Node* cmd = CMD(NULL);
    ToolchainType toolchain = c_toolchain_select_toolchain_automatically();
    set_default_toolchain(toolchain);
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        bootstrap_compile_link_make_cmdline_msvc(cmd, self);
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_LLVM || toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        bootstrap_compile_link_make_cmdline_llvm_gcc_zig(cmd, self);
    }
    cmd->extra_data = self;
    cmd->before_execute = bootstrap_compile_link_cmd_before_execute;
    node_ensure_prepared(cmd);
    return build_self();
}

void init_mode(void)
{
    set_var("test_src_dir", get_var("out_dir"));

    extern bool b_bootstrap;
    if (b_bootstrap)
    {
        int exit_code = bootstrap();
        exit(exit_code);
    }
}
