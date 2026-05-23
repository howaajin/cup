#include "cup/c_toolchain/c_compile_cmd.h"

#include "core/allocator.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/c_toolchain/scan_deps_cmd.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include <assert.h>

extern Allocator* node_allocator;
extern ToolchainType default_toolchain;
extern CLanguageStandard default_c_std;
extern CppLanguageStandard default_cpp_std;
extern OptimizationType default_optimization_type;
extern ArchitectureType default_architecture_type;
extern bool b_generate_debug_info;
extern bool b_cache_header_dependencies;

void c_compile_cmd_prepare_gcc(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_msvc(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_zigcc(Node* node, CCompileCmd* cmd);

void compile_cmdline_node_make_cmdline_gcc(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_msvc(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_zigcc(CompileCmdline* compile_cmdline);

extern SourceType get_source_type(char const* path);

char const* get_toolchain_bmi_extension(ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        return ".ifc";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM || toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        return ".pcm";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_GCC)
    {
        return ".gcm";
    }
    assert(false);
    return NULL;
}

void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type)
{
    for (uint32_t i = set->begin; i != set->end; i = hash_next(set, i))
    {
        char const* param = hash_key(set, i);
        cmd_add_option(node, option, param, option_type);
    }
}

void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type)
{
    for (size_t i = 0; i != array_size(set); i++)
    {
        char const* param = set[i];
        cmd_add_option(node, option, param, option_type);
    }
}

void compile_cmdline_node_make_cmdline(CompileCmdline* node)
{
    ToolchainType toolchain = node->cmd->toolchain;
    switch (toolchain)
    {
    case TOOLCHAIN_TYPE_LLVM: compile_cmdline_node_make_cmdline_llvm(node); break;
    case TOOLCHAIN_TYPE_GCC: compile_cmdline_node_make_cmdline_gcc(node); break;
    case TOOLCHAIN_TYPE_MSVC: compile_cmdline_node_make_cmdline_msvc(node); break;
    case TOOLCHAIN_TYPE_ZIG: compile_cmdline_node_make_cmdline_zigcc(node); break;
    default: assert(false);
    }
}

static void compile_cmdline_node_visit(Node* node, Graph* graph, Executor* executor)
{
    if (node->b_dirty)
    {
        CompileCmdline* compile_cmdline = (CompileCmdline*)node;
        compile_cmdline_node_make_cmdline(compile_cmdline);
        node->b_dirty = false;
    }
    node->processed(node, graph);
}

static Node* compile_cmdline_node_create(CCompileCmd* c_compile_cmd)
{
    char const* name = fmt("make_cmdline: {}", c_compile_cmd->name);
    uint32_t type = node_make_virtual_type(VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE);
    Node* node = node_create(type, name, sizeof(CompileCmdline));
    CompileCmdline* cmdline_node = (CompileCmdline*)node;
    node->visit = compile_cmdline_node_visit;
    node->b_dirty = true;
    cmdline_node->cmd = c_compile_cmd;
    return node;
}

void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map)
{
    for (size_t i = 0; i != array_size(cmd->import_names); i++)
    {
        char const* name = cmd->import_names[i];
        Node* bmi = cmd->import_bmis[i];
        if (string_equal(name, "std"))
        {
            if (bmi == NULL)
                bmi = cmd->import_bmis[i] = get_or_create_std_module_for_compile_cmd(cmd);
            cmd->b_import_std = true;
        }
        if (string_equal(name, "std.compat"))
        {
            if (bmi == NULL)
                bmi = cmd->import_bmis[i] = get_or_create_std_compat_module_for_compile_cmd(cmd);
            cmd->b_import_std = true;
        }
        if (bmi == NULL)
        {
            continue;
        }
        bool b_existed;
        uint32_t index = hash_insert_check(out_map, name, &b_existed);
        if (!b_existed)
        {
            hash_value(out_map, index) = bmi;
            if (bmi->build_cmd)
            {
                c_compile_cmd_get_all_imports((CCompileCmd*)bmi->build_cmd, out_map);
            }
        }
    }
}

static void c_compile_cmd_add_module_inputs(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (size_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Node* bmi = hash_value(h, i);
        cmd_add_input(node, bmi);
        if (bmi->build_cmd)
        {
            CCompileCmd* bmi_build_cmd = (CCompileCmd*)bmi->build_cmd;
            obj_add_link_node(cmd->out_obj, bmi_build_cmd->out_obj);
        }
    }
}

static void c_compile_cmd_prepare(Node* node)
{
    CCompileCmd* cmd = (CCompileCmd*)node;

    node_add_dependency(node, node->make_cmdline);
    cmd_add_input(node, cmd->src);

    if (cmd->b_cpp)
    {
        if (cmd->export_bmi || cmd->export_name)
        {
            cmd->source_type = SOURCE_TYPE_CPPM;
        }
        if (cmd->source_type == SOURCE_TYPE_CPPM)
        {
            if (cmd->export_bmi == NULL)
            {
                cmd->export_bmi = module_from_src(cmd->src);
                cmd_add_output(node, cmd->export_bmi);
            }
        }
        c_compile_cmd_add_module_inputs(node, cmd);
    }
    switch (cmd->toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC: c_compile_cmd_prepare_msvc(node, cmd); break;
    case TOOLCHAIN_TYPE_LLVM: c_compile_cmd_prepare_llvm(node, cmd); break;
    case TOOLCHAIN_TYPE_GCC: c_compile_cmd_prepare_gcc(node, cmd); break;
    case TOOLCHAIN_TYPE_ZIG: c_compile_cmd_prepare_zigcc(node, cmd); break;
    default: assert(false);
    }

    cmd_prepare(node);
}

bool c_compile_cmd_check_dirty(Node* node)
{
    if (cmd_check_dirty(node))
    {
        return true;
    }
    CCompileCmd* cmd = (CCompileCmd*)node;
    if (cmd->scan_deps_cmd && cmd->scan_deps_cmd->b_dirty)
    {
        return true;
    }
    return false;
}

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line)
{
    assert(input);
    assert(out_obj);
    assert(out_obj->build_cmd == NULL);

    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE);
    char const* name = fmt("compile: {:n}", out_obj);
    Node* node = node_create(type, name, sizeof(CCompileCmd));
    CCompileCmd* cmd = (CCompileCmd*)node;
    {
        SourceType source_type = get_source_type(input->path);
        cmd->toolchain = default_toolchain;
        cmd->includes = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        cmd->includes->allocator = node_allocator;
        cmd->defines = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        cmd->defines->allocator = node_allocator;
        cmd->src = input;
        cmd->out_obj = out_obj;
        cmd->source_type = source_type;
        cmd->b_cpp = source_type == SOURCE_TYPE_CPP || source_type == SOURCE_TYPE_CPPM;
        cmd->make_cmdline = compile_cmdline_node_create(cmd);
        cmd->prepare = c_compile_cmd_prepare;
        cmd->check_dirty = c_compile_cmd_check_dirty;
        cmd->b_color_diagnostics = os_is_terminal_supports_color();
        cmd->b_generate_debug_info = b_generate_debug_info;
        cmd->b_cache_header_dependencies = b_cache_header_dependencies;
        cmd->c_std = default_c_std;
        cmd->cpp_std = default_cpp_std;
        cmd->optimization_type = default_optimization_type;
        cmd->arch = default_architecture_type;
    }
    out_obj->build_cmd = node;
    node->file = file;
    node->line = line;
    return node;
}

void c_compile_cmd_add_include_directory(Node* node, char const* dir)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    bool b_existed;
    uint32_t i = hash_insert_check(cmd->includes, dir, &b_existed);
    if (!b_existed)
    {
        hash_key(cmd->includes, i) = string_from_c_str(node_allocator, dir);
    }
}

void c_compile_cmd_add_define(Node* node, char const* define)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    bool b_existed;
    uint32_t i = hash_insert_check(cmd->defines, define, &b_existed);
    if (!b_existed)
    {
        hash_key(cmd->defines, i) = string_from_c_str(node_allocator, define);
    }
}

void c_compile_cmd_add_flag(Node* node, char const* flag)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    char* f = string_from_c_str(node_allocator, flag);
    array_push(node_allocator, cmd->flags, f);
}

void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std)
{
    CCompileCmd* cc = (CCompileCmd*)cmd;
    cc->c_std = c_std;
}

void c_compile_cmd_set_cpp_std(Node* cmd, CppLanguageStandard cpp_std)
{
    CCompileCmd* cc = (CCompileCmd*)cmd;
    cc->cpp_std = cpp_std;
}

void c_compile_cmd_set_arch(Node* node, ArchitectureType arch)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->arch = arch;
}

void c_compile_cmd_set_optimization_type(Node* node, OptimizationType type)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->optimization_type = type;
}

void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    if (!string_equal(name, "std") && !string_equal(name, "std.compat") && bmi == NULL)
    {
        warn("c_compile_cmd_add_import: named module: %s must not empty", name);
        return;
    }
    char* import_name = string_from_c_str(node_allocator, name);
    array_push(node_allocator, cmd->import_names, import_name);
    array_push(node_allocator, cmd->import_bmis, bmi);
}

void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->export_bmi = bmi;
    cmd->export_bmi->build_cmd = node;
    c_compile_cmd_set_export_name(node, name);
}

void c_compile_cmd_set_export_name(Node* node, char const* name)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    array_resize(node_allocator, cmd->export_name, 0);
    if (name)
    {
        string_concat_c_str(node_allocator, cmd->export_name, name);
    }
}

void c_compile_cmd_set_export_map(CCompileCmd* cmd, StringPtrHash* map)
{
    cmd->export_map = map;
}

void c_compile_cmd_set_import_map(CCompileCmd* cmd, StringPtrHash* map)
{
    cmd->import_map = map;
}

void c_compile_cmd_add_self_build_options(Node* node)
{
    extern ToolchainType self_build_toolchain;

    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->b_self_build = true;
    cmd->toolchain = self_build_toolchain;
    c_compile_cmd_set_arch(node, ARCH_X64);
    if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        c_compile_cmd_set_c_std(node, C_LANGUAGE_STANDARD_23);
    }
    else
    {
        if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM || cmd->toolchain == TOOLCHAIN_TYPE_ZIG)
        {
            c_compile_cmd_add_flag(node, "-Wno-microsoft-anon-tag");
        }
        c_compile_cmd_add_flag(node, "-fms-extensions");
        c_compile_cmd_add_flag(node, "-Wno-deprecated-declarations");
    }
}
