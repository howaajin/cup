#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include <assert.h>

extern Allocator* node_allocator;

typedef struct ModuleMapper ModuleMapper;

struct ModuleMapper
{
    CCompileCmd* cmd;
    char const* content;
    Node* file;
};

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd);
void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);
char const* get_arch_option_clang_or_gcc(ArchitectureType arch);

static void compile_cmdline_node_make_cmdline_gcc_c_cpp_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (array_size(cmd->export_name) || array_size(cmd->import_names))
    {
        cmd_add_option(node, "-fmodules", NULL, OPTION_FLAG);
        cmd_add_input_file_option(node, "-fmodule-mapper=", cmd->module_mapper);
    }
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, "-fdiagnostics-color", NULL, OPTION_HIDDEN);
    }
}

static void compile_cmdline_node_make_cmdline_gcc_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "g++", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_gcc_c_cpp_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_gcc_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "gcc", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_gcc_c_cpp_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_gcc_cpp_module(CompileCmdline* compile_cmdline)
{
    compile_cmdline_node_make_cmdline_gcc_cpp(compile_cmdline);
}

static void compile_cmdline_node_make_cmdline_gcc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, NULL, "gcc", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->arch)
    {
        char const* arch = get_arch_option_clang_or_gcc(cmd->arch);
        cmd_add_option(node, arch, NULL, OPTION_FLAG);
    }
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, "-g", NULL, OPTION_FLAG);
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    if (!b_pure_asm && cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void compile_cmdline_node_make_cmdline_gcc(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM || cmd->export_name)
    {
        compile_cmdline_node_make_cmdline_gcc_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_gcc_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_gcc_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_gcc_asm(compile_cmdline);
    }
    else
    {
        assert(false);
    }
}

void compile_cmd_after_execute_llvm_gcc(Node* node);
void c_compile_cmd_llvm_gcc_setup_after_execute_fn(Node* node, CCompileCmd* cmd, void (*fn)(Node*));
char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator);

static char* module_mapper_to_string(ModuleMapper* data, Allocator* allocator)
{
    char* result = NULL;
    CCompileCmd* cmd = data->cmd;
    for (size_t i = 0; i != array_size(cmd->import_names); i++)
    {
        char const* name = cmd->import_names[i];
        Node* bmi = cmd->import_bmis[i];
        if (bmi == NULL)
        {
            continue;
        }
        string_printf(allocator, result, "%s %s\n", name, bmi->path);
    }
    if (array_size(cmd->export_name))
    {
        string_printf(allocator, result, "%s %s\n", cmd->export_name, cmd->export_bmi->path);
    }
    return result;
}

static bool module_mapper_gen_cmd_check_dirty(Node* node)
{
    if (cmd_check_dirty(node))
    {
        return true;
    }
    ModuleMapper* mapper = node->extra_data;
    Node* output = mapper->file;
    mapper->content = module_mapper_to_string(mapper, allocator_c());
    char const* old_content = os_read_all(allocator_temp(), output->path);
    if (!string_equal(old_content, mapper->content))
    {
        return true;
    }
    return false;
}

static int module_mapper_gen_cmd_thread_fn(Node* node)
{
    ModuleMapper* mapper = node->extra_data;
    Node* output = mapper->file;
    char const* content = mapper->content;
    if (content == NULL)
    {
        content = module_mapper_to_string(mapper, allocator_c());
    }
    else
    {
        mapper->content = NULL;
    }
    os_write_all(output->path, content, array_size(content));
    if (content)
    {
        array_free(allocator_c(), content);
    }
    return EXIT_SUCCESS;
}

static Node* module_mapper_gen_cmd_create(Node* output, CCompileCmd* cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_THREAD, 0);
    char const* name = fmt("gen: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    ModuleMapper* mapper = allocator_calloc(node_allocator, 1, sizeof(ModuleMapper));
    mapper->cmd = cmd;
    mapper->file = output;
    node->extra_data = mapper;
    node->check_dirty = module_mapper_gen_cmd_check_dirty;
    node->fn = module_mapper_gen_cmd_thread_fn;
    cmd_add_output(node, output);
    return node;
}

void c_compile_cmd_prepare_gcc(Node* node, CCompileCmd* cmd)
{
    if (cmd->export_name && cmd->export_bmi == NULL)
    {
        cmd->export_bmi = module_from_src(cmd->src);
    }
    if (cmd->export_bmi)
    {
        cmd_add_output(node, cmd->export_bmi);
    }
    if (cmd->export_bmi || array_size(cmd->import_names))
    {
        if (cmd->module_mapper == NULL)
        {
            char const* mapper_path = determine_imtermediate_path(cmd->src->path, "mappers", ".txt", allocator_temp());
            cmd->module_mapper = get_or_add_file(mapper_path);
            module_mapper_gen_cmd_create(cmd->module_mapper, cmd);
            cmd_add_input(node, cmd->module_mapper);
        }
    }
    cmd_add_output(node, cmd->out_obj);
    if (cmd->scan_deps_cmd == NULL && cmd->b_cache_header_dependencies)
    {
        c_compile_cmd_llvm_gcc_setup_after_execute_fn(node, cmd, compile_cmd_after_execute_llvm_gcc);
    }
}
