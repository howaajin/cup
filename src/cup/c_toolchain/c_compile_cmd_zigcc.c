#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/node.h"

#include <assert.h>

void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd);
bool is_clang_supported_module_extension(char const* path);
void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

void c_compile_cmd_prepare_zigcc(Node* node, CCompileCmd* cmd)
{
    extern char* g_zig_target;
    if (cmd->toolchain == TOOLCHAIN_TYPE_ZIG && g_zig_target)
    {
        c_compile_cmd_add_flag(node, fmt("-target {}", g_zig_target));
    }
    c_compile_cmd_prepare_llvm(node, cmd);
}

void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline);

static void compile_cmdline_node_make_cmdline_zigcc_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, "-fcolor-diagnostics", NULL, OPTION_HIDDEN);
        cmd_add_option(node, "-fansi-escape-codes", NULL, OPTION_HIDDEN);
        cmd_add_option(node, "-fdiagnostics-color=always", NULL, OPTION_HIDDEN);
    }
}

static void compile_cmdline_node_make_cmdline_zigcc_cpp_module(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig c++", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->export_bmi);
    cmd_add_option(node, "--precompile", NULL, OPTION_FLAG);
    if (!is_clang_supported_module_extension(cmd->src->path))
    {
        cmd_add_option(node, "-x c++-module", NULL, OPTION_FLAG);
    }
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

void compile_cmdline_node_make_cmdline_zigcc_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig cc", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_zigcc_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig c++", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_zigcc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, NULL, "zig cc", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->arch)
    {
        char const* arch_opt = NULL;
        if (cmd->arch == ARCH_X64) arch_opt = "-m64";
        else if (cmd->arch == ARCH_X86) arch_opt = "-m32";
        if (arch_opt)
            cmd_add_option(node, arch_opt, NULL, OPTION_FLAG);
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

void compile_cmdline_node_make_cmdline_zigcc(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_zigcc_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_zigcc_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_zigcc_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_zigcc_asm(compile_cmdline);
    }
    else
    {
        assert(false);
    }
}
