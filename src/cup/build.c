#include "core/macros.h"
#include "cup/cup.h"
static struct
{
    char const* path;
    char const* out_path;
} embedded_sources[] = {
    {.path = "src/cup/test.h", .out_path = "src/cup/bin2c/test.h.c"},
    {.path = "src/cup/test.c", .out_path = "src/cup/bin2c/test.c.c"},
    {.path = "src/cup/test_main.c", .out_path = "src/cup/bin2c/test_main.c.c"},
    {.path = "src/cup/build_script_tpl.c", .out_path = "src/cup/bin2c/build_script_tpl.c.c"},
};

ENTRY(build_bin2c)
{
    Node* src = SRC("{dir}/bin2c.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    Node* exe = EXE("{out_dir}/bin2c");
    Node* link = LINK(exe);
    link_cmd_setup_self_build(link);
    link_cmd_add_input(link, obj);
    node_add_debugger_argument(exe, "build/test.c");
    node_add_debugger_argument(exe, "src/cup/bin2c.c");
}

ENTRY(gen_embedded_files)
{
    Node* bin2c = EXE("{out_dir}/bin2c");
    for (size_t i = 0; i != static_array_size(embedded_sources); i++)
    {
        Node* output = get_or_add_src(embedded_sources[i].out_path);
        Node* cmd = CMD_FROM_EXE(bin2c, fmt("gen: {:n}", output));
        Node* input = get_or_add_src(embedded_sources[i].path);
        cmd_add_option(cmd, OPTION_FLAG, "-base64");
        cmd_add_output_file_option(cmd, output);
        cmd_add_input_file_option(cmd, input);
    }
}

ENTRY(build_gen_cup_h)
{
    Node* gen_cup_h = EXE("{out_dir}/gen_cup_h");
    Node* link = LINK(gen_cup_h);
    link_cmd_setup_self_build(link);
    {
        link_cmd_set_arch(link, get_self_build_arch());
        Node* src = get_or_add_src("src/cup/gen_cup_h.c");
        Node* obj = OBJ(src);
        Node* cc = CC(src, obj);
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
        }
        link_cmd_add_input(link, obj);
        link_cmd_add_input(link, LIB("{out_dir}/allocator"));
    }
}

ENTRY(build_gen_def)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    Node* gen_def = EXE("{out_dir}/gen_def");
    Node* link = LINK(gen_def);
    link_cmd_setup_self_build(link);
    {
        link_cmd_set_arch(link, get_self_build_arch());
        Node* src = get_or_add_src("src/cup/gen_def.c");
        Node* obj = OBJ(src);
        Node* cc = CC(src, obj);
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
        }
        link_cmd_add_input(link, obj);
        link_cmd_add_input(link, LIB("{out_dir}/allocator"));
    }
}

static void gen_cup_h_output_filter(Node* cmd, char const* line)
{
    cmd_add_implicit_input(cmd, line);
}

ENTRY(build_cup_h)
{
    Node* amalgam = get_or_add_src("src/cup/amalgam.c");
    Node* cup_h = FILE("{out_dir}/header_only/cup.h");
    Node* gen_cup_h = EXE("{out_dir}/gen_cup_h");
    Node* cmd = CMD_FROM_EXE(gen_cup_h, fmt("gen: {:n}", cup_h));
    cmd_add_option(cmd, OPTION_FLAG, "-o");
    cmd_add_output_file_option(cmd, cup_h);
    cmd_add_option(cmd, OPTION_INPUT, "cup/amalgam.c");
    cmd_add_input(cmd, amalgam);
    for (size_t i = 0; i != static_array_size(embedded_sources); i++)
    {
        Node* src = get_or_add_src(embedded_sources[i].out_path);
        cmd_add_input(cmd, src);
    }
    cmd_set_write_output_line_fn(cmd, gen_cup_h_output_filter);
}

static void self_with_source_built_restart(Node* cmd)
{
    cmd_after_execute(cmd);
    // exit(0);
    restart();
}

char const* cup_common_sources[] = {
    "{dir}/cup.c",
    "{dir}/c_toolchain/c_toolchain.c",
    "{platform_c_toolchain_c}",
    "{dir}/c_toolchain/c_compile_cmd.c",
    "{dir}/c_toolchain/c_compile_cmd_gcc.c",
    "{dir}/c_toolchain/c_compile_cmd_llvm.c",
    "{dir}/c_toolchain/c_compile_cmd_zigcc.c",
    "{dir}/c_toolchain/c_compile_cmd_msvc.c",
    "{dir}/c_toolchain/link_cmd.c",
    "{dir}/c_toolchain/ar_cmd.c",
    "{dir}/c_toolchain/gen_compile_commands.c",
    "{dir}/c_toolchain/scan_test.c",
    "{dir}/embedded_file.c",
    "{dir}/gen_build_c.c",
    "{dir}/bin2c/build_script_tpl.c.c",
    "{dir}/cache.c",
    "{dir}/depfile.c",
    "{dir}/entry.c",
    "{dir}/graph.c",
    "{dir}/node.c",
    "{dir}/test_finder.c",
    "{dir}/fmt.c",
};

ENTRY(build_cup_lib)
{
    Node* sources[] = {
        SRC("{dir}/executor/executor.c"),
        SRC("{dir}/executor/executor_{platform}.c"),
        SRC("{dir}/c_toolchain/c_toolchain.c"),
        SRC("{platform_c_toolchain_c}"),
        SRC("{dir}/c_toolchain/c_compile_cmd.c"),
        SRC("{dir}/c_toolchain/c_compile_cmd_gcc.c"),
        SRC("{dir}/c_toolchain/c_compile_cmd_llvm.c"),
        SRC("{dir}/c_toolchain/c_compile_cmd_zigcc.c"),
        SRC("{dir}/c_toolchain/c_compile_cmd_msvc.c"),
        SRC("{dir}/c_toolchain/link_cmd.c"),
        SRC("{dir}/c_toolchain/ar_cmd.c"),
        SRC("{dir}/c_toolchain/cpp_module.c"),
        SRC("{dir}/c_toolchain/scan_test.c"),
        SRC("{dir}/embedded_file.c"),
        SRC("{dir}/bin2c/build_script_tpl.c.c"),
        SRC("{dir}/cache.c"),
        SRC("{dir}/depfile.c"),
        SRC("{dir}/entry.c"),
        SRC("{dir}/graph.c"),
        SRC("{dir}/node.c"),
        SRC("{dir}/var.c"),
        SRC("{dir}/test_finder.c"),
        SRC("{dir}/fmt.c"),
    };
    Node* cup_lib = LIB("{out_dir}/cup");
    Node* cmd = AR(cup_lib);
    for (size_t i = 0; i != static_array_size(sources); i++)
    {
        Node* obj = get_default_obj(sources[i]);
        ar_cmd_add_input(cmd, obj);
    }
}

ENTRY(build_embedded_file)
{
    Node* src = SRC("{dir}/embedded_file.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_gen_build_c)
{
    Node* src = SRC("{dir}/gen_build_c.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_cup)
{
    Node* src = SRC("{dir}/cup.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/core/utilities.c"));
    obj_add_link_obj_from_src(obj, SRC("{platform_directory_c}"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/executor/executor.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/c_toolchain/c_toolchain.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/c_toolchain/scan_test.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/node.c"));
}

ENTRY(build_cache)
{
    Node* src = SRC("{dir}/cache.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_depfile)
{
    Node* src = SRC("{dir}/depfile.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_entry)
{
    Node* src = SRC("{dir}/entry.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_graph)
{
    Node* src = SRC("{dir}/graph.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_node)
{
    Node* src = SRC("{dir}/node.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/graph.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/executor/executor.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/fmt.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/cache.c"));
}

ENTRY(build_test_finder)
{
    Node* src = SRC("{dir}/test_finder.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_fmt)
{
    Node* src = SRC("{dir}/fmt.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/var.c"));
}

ENTRY(build_executor)
{
    Node* src = SRC("{dir}/executor/executor.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/allocator"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/executor/executor_{platform}.c"));
}

ENTRY(build_platform_executor)
{
    Node* src = SRC("{dir}/executor/executor_{platform}.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/allocator"));
    obj_add_link_obj_from_src(obj, SRC("src/core/os.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/executor/executor.c"));
}

ENTRY(build_vs)
{
    Node* src = SRC("{dir}/vs.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_self_with_source)
{
    extern char** build_script_search_directories;
    extern void collect_build_scripts(char const* directory, Allocator* allocator);

    Allocator* allocator = allocator_temp();
    add_build_script("build.c");
    for (size_t i = 0; i != array_size(build_script_search_directories); i++)
    {
        collect_build_scripts(build_script_search_directories[i], allocator);
    }
    Node* self = EXE("{self_name}");
    Node* link = LINK(self);
    link_cmd_setup_self_build(link);
    link_cmd_set_arch(link, get_self_build_arch());

    Node* cup_lib = LIB("{out_dir}/cup");
    link_cmd_add_input(link, cup_lib);
    Node* core_lib = LIB("{out_dir}/core");
    link_cmd_add_input(link, core_lib);

    Node** references = NULL;
    array_push(allocator_temp(), references, SRC("{dir}/in_repo.c"));
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        Node* pdb = FILE("{out_dir}/cup.exe.pdb");
        link_cmd_set_pdb(link, pdb);
        // icon
        Node* res = FILE("{out_dir}/cup.res");
        link_cmd_add_input(link, res);
        // vs project generator
        array_push(allocator_temp(), references, SRC("{dir}/vs.c"));
    }
    for (size_t i = 0; i != static_array_size(cup_common_sources); i++)
    {
        array_push(allocator_temp(), references, SRC(cup_common_sources[i]));
    }
    for (size_t i = 0; i != array_size(references); i++)
    {
        Node* obj = OBJ(references[i]);
        link_cmd_add_input(link, obj);
    }

    extern StringSet* build_scripts;
    for (uint32_t i = build_scripts->begin; i != build_scripts->end; i = hash_next(build_scripts, i))
    {
        Node* src = SRC(hash_key(build_scripts, i));
        Node* obj = OBJ(src);
        CC(src, obj);
        link_cmd_add_input(link, obj);
    }
    cmd_set_after_execute_fn(link, self_with_source_built_restart);
}

ENTRY(build_cup_h_no_impl)
{
    Node* amalgam = get_or_add_src("src/cup/amalgam.c");
    Node* cup_h = FILE("{out_dir}/embedded/cup.h");
    Node* gen_cup_h = EXE("{out_dir}/gen_cup_h");
    Node* cmd = CMD_FROM_EXE(gen_cup_h, fmt("gen: {:n}", cup_h));
    cmd_add_option(cmd, OPTION_FLAG, "-o");
    cmd_add_output_file_option(cmd, cup_h);
    cmd_add_option(cmd, OPTION_INPUT, "cup/amalgam.c");
    cmd_add_option(cmd, OPTION_FLAG, "-h");
    cmd_add_input(cmd, amalgam);
    for (size_t i = 0; i != static_array_size(embedded_sources); i++)
    {
        Node* src = SRC(embedded_sources[i].out_path);
        cmd_add_input(cmd, src);
    }
    cmd_set_write_output_line_fn(cmd, gen_cup_h_output_filter);
}

ENTRY(build_cup_def)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    Node* cup_h = FILE("{out_dir}/embedded/cup.h");
    Node* cup_def = FILE("{out_dir}/cup.def");
    Node* gen_def = EXE("{out_dir}/gen_def");
    Node* cmd = CMD_FROM_EXE(gen_def, fmt("gen: {:n}", cup_def));
    cmd_add_option(cmd, OPTION_FLAG, "-o");
    cmd_add_output_file_option(cmd, cup_def);
    cmd_add_input_file_option(cmd, cup_h);
}

ENTRY(build_embedded_cup_h_c)
{
    Node* bin2c = EXE("{out_dir}/bin2c");
    Node* output = SRC("{out_dir}/embedded/cup.h.c");
    Node* cmd = CMD_FROM_EXE(bin2c, fmt("gen: {:n}", output));
    Node* input = FILE("{out_dir}/embedded/cup.h");
    cmd_add_option(cmd, OPTION_FLAG, "-base64");
    cmd_add_output_file_option(cmd, output);
    cmd_add_input_file_option(cmd, input);
}

ENTRY(build_embedded_cup_def_c)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    Node* bin2c = EXE("{out_dir}/bin2c");
    Node* output = SRC("{out_dir}/embedded/cup.def.c");
    Node* cmd = CMD_FROM_EXE(bin2c, fmt("gen: {:n}", output));
    Node* input = FILE("{out_dir}/cup.def");
    cmd_add_option(cmd, OPTION_FLAG, "-base64");
    cmd_add_output_file_option(cmd, output);
    cmd_add_input_file_option(cmd, input);
}

ENTRY(build_gen_test_src)
{
    Node* src = SRC("{dir}/gen_test_src.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_build_script_tpl)
{
    Node* src = SRC("{dir}/bin2c/build_script_tpl.c.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_cup_embedded)
{
    Node* exe = EXE("{out_dir}/embedded/{self_name}");
    Node* link = LINK(exe);
    link_cmd_setup_self_build(link);
    link_cmd_set_arch(link, get_self_build_arch());
    Node* def = FILE("{out_dir}/cup.def");
    link_cmd_set_def_file(link, def);
    for (size_t i = 0; i != static_array_size(cup_common_sources); i++)
    {
        Node* src = SRC(cup_common_sources[i]);
        Node* obj = OBJ(src);
        link_cmd_add_input(link, obj);
    }
    {
        Node* src = SRC("{dir}/gen_test_src.c");
        Node* obj = OBJ(src);
        link_cmd_add_input(link, obj);
    }
    Node* sources[] = {
        SRC("{dir}/dllmain.c"),
        SRC("{dir}/bin2c/test.c.c"),
        SRC("{dir}/bin2c/test.h.c"),
        SRC("{dir}/bin2c/test_main.c.c"),
        SRC("{out_dir}/embedded/cup.h.c"),
    };
    for (size_t i = 0; i != static_array_size(sources); i++)
    {
        Node* src = sources[i];
        Node* obj = OBJ(src);
        CC(src, obj);
        link_cmd_add_input(link, obj);
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        // embedded def
        Node* src_def = SRC("{out_dir}/embedded/cup.def.c");
        Node* obj_def = OBJ(src_def);
        CC(src_def, obj_def);
        link_cmd_add_input(link, obj_def);

        // icon
        Node* res = FILE("{out_dir}/cup.res");
        link_cmd_add_input(link, res);

        // vs project generator
        Node* src_vs = SRC("{dir}/vs.c");
        Node* obj_vs = OBJ(src_vs);
        link_cmd_add_input(link, obj_vs);
    }
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        link_cmd_add_flag(link, "-rdynamic");
    }

    Node* copied = EXE("examples/{self_name}");
    COPY(exe, copied);
}

ENTRY(build_gen_icon_exe)
{
    Node* src = SRC("{dir}/gen_icon.c");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    c_compile_cmd_add_self_build_options(cc);
    c_compile_cmd_add_define(cc, fmt("CUP_VERSION_MAJOR={:d}", CUP_VERSION[0] - '0'));
    Node* link = LINK(EXE("{out_dir}/gen_icon"));
    link_cmd_setup_self_build(link);
    link_cmd_add_input(link, obj);
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        link_cmd_add_lib(link, "m");
    }
}

ENTRY(build_cup_icon)
{
    Node* icon = FILE("{out_dir}/icon.ico");
    Node* readme_png = FILE("assets/icon.png");
    Node* gen_icon = EXE("{out_dir}/gen_icon");
    Node* cmd = CMD_FROM_EXE(gen_icon, fmt("gen: {:n}", icon));
    cmd_add_output_file_option(cmd, icon);
    cmd_add_output_file_option(cmd, readme_png);
}

ENTRY(build_cup_res)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    Node* res = FILE("{out_dir}/cup.res");
    ToolchainType toolchain_type = get_default_toolchain();
    if (toolchain_type == TOOLCHAIN_TYPE_GCC)
    {
        Node* cmd = CMD("windres");
        Node* icon = FILE("{out_dir}/icon.ico");
        Node* rc = FILE("{dir}/cup.rc");
        cmd_add_option(cmd, OPTION_FLAG, "-o");
        cmd_add_output_file_option(cmd, res);
        cmd_add_input_file_option(cmd, rc);
        cmd_add_option(cmd, OPTION_FLAG, "-O coff");
        cmd_add_input(cmd, icon);
    }
    else
    {
        char const* rc_name = NULL;
        if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
        {
            rc_name = "rc";
        }
        if (toolchain_type == TOOLCHAIN_TYPE_LLVM)
        {
            rc_name = "llvm-rc";
        }
        if (toolchain_type == TOOLCHAIN_TYPE_ZIG)
        {
            rc_name = "zig rc";
        }
        expect(rc_name, "rc_name is NULL");
        extern ToolchainType self_build_toolchain;
        Node* cmd = CMD(rc_name);
        Node* env = get_toolchain_env_node(self_build_toolchain, get_self_build_arch());
        cmd_set_env(cmd, env);
        Node* icon = FILE("{out_dir}/icon.ico");
        Node* rc = FILE("{dir}/cup.rc");
        cmd_add_option(cmd, OPTION_FLAG, "/nologo");
        cmd_add_option(cmd, OPTION_FLAG, "/fo");
        cmd_add_output_file_option(cmd, res);
        cmd_add_input_file_option(cmd, rc);
        cmd_add_input(cmd, icon);
    }
}

ENTRY(build_var)
{
    Node* src = SRC("{dir}/var.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_in_repo)
{
    Node* src = SRC("{dir}/in_repo.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_toolchain.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/cup.c"));
}
