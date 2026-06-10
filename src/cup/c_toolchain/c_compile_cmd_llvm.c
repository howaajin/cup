#include "cup/c_toolchain/c_compile_cmd.h"

#include "core/allocator.h"
#include "core/hash.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/depfile.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include "core/macros.h"
#include <assert.h>
#include <stdio.h>

extern ToolchainType default_toolchain;

void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

char const* get_arch_option_clang_or_gcc(ArchitectureType arch)
{
    if (arch == ARCH_UNSPECIFIED) return NULL;
    if (arch == ARCH_X64) return "-m64";
    if (arch == ARCH_X86) return "-m32";
    return NULL;
}

static char const* get_optimization_option_clang_or_gcc(OptimizationType optimization)
{
    switch (optimization)
    {
    case OPTIMIZATION_TYPE_UNSPECIFIED: return NULL;
    case OPTIMIZATION_TYPE_DEBUG: return "-O0";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "-O3";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "-Os";
    default: fatal("unknown optimization type"); return NULL;
    }
}

static char const* get_cpp_std_option_clang_or_gcc(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case CPP_LANGUAGE_STANDARD_98: return "-std=c++98";
    case CPP_LANGUAGE_STANDARD_11: return "-std=c++11";
    case CPP_LANGUAGE_STANDARD_14: return "-std=c++14";
    case CPP_LANGUAGE_STANDARD_17: return "-std=c++17";
    case CPP_LANGUAGE_STANDARD_20: return "-std=c++20";
    case CPP_LANGUAGE_STANDARD_23: return "-std=c++23";
    case CPP_LANGUAGE_STANDARD_26: return "-std=c++26";
    default: fatal("unknown C++ language standard"); return NULL;
    }
}

static char const* get_c_std_option_clang_or_gcc(CLanguageStandard c_std)
{
    switch (c_std)
    {
    case C_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case C_LANGUAGE_STANDARD_99: return "-std=c99";
    case C_LANGUAGE_STANDARD_11: return "-std=c11";
    case C_LANGUAGE_STANDARD_17: return "-std=c17";
    case C_LANGUAGE_STANDARD_23: return "-std=c2x";
    default: fatal("unknown C language standard"); return NULL;
    }
}

bool is_clang_supported_module_extension(char const* path)
{
    Allocator* allocator = allocator_temp();
    char* ext = (char*)path_extension(path);
    ext = string_from_c_str(allocator, ext);
    string_tolower(ext);
    if (string_equal(ext, ".cppm") || string_equal(ext, ".cxxm") || string_equal(ext, "ccm"))
    {
        return true;
    }
    return false;
}

char const* c_compile_cmd_get_depfile_path(CCompileCmd* cmd)
{
    char const* depfile_path;
    if (file_path_has_space(cmd->out_obj))
    {
        depfile_path = fmt("\"{:n}.d\"", cmd->out_obj);
    }
    else
    {
        depfile_path = fmt("{:n}.d", cmd->out_obj);
    }
    return depfile_path;
}

void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd)
{
    char const* depfile_path = c_compile_cmd_get_depfile_path(cmd);
    cmd_add_option(node, OPTION_HIDDEN, "-MMD -MF");
    cmd_add_option(node, OPTION_HIDDEN, depfile_path);
}

void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, OPTION_FLAG, "-o");
    cmd_add_output_file_option(node, cmd->out_obj);
    cmd_add_option(node, OPTION_FLAG, "-c");
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

static void compile_cmd_before_execute_rename_occupied_cc_file(Node* cmd)
{
    void c_toolchain_rename_to_old(char const* path);

    cmd_before_execute(cmd);
    CCompileCmd* cc = (CCompileCmd*)cmd;
    if (cc->export_bmi)
    {
        if (!os_file_writable(cc->export_bmi->path))
        {
            c_toolchain_rename_to_old(cc->export_bmi->path);
        }
    }
}

void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
static Node* bmi_to_obj_cmd_create(CCompileCmd* cc, char const* file, int line)
{
    extern bool b_generate_debug_info;

    Node* obj = cc->out_obj;
    Node* pcm = cc->export_bmi;
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_BMI_TO_OBJ);
    Node* cmd = node_create(node_type, fmt("gen: {:n}", obj), sizeof(BmiToObjCmd));
    cmd_set_source_location(cmd, file, line);
    BmiToObjCmd* cmd_bmi_to_obj = (BmiToObjCmd*)cmd;
    cmd_bmi_to_obj->c_compile_cmd = cc;
    char const* compiler = default_toolchain == TOOLCHAIN_TYPE_ZIG ? "zig c++" : get_clang_cpp_compiler();
    cmd_add_option(cmd, OPTION_EXE, compiler);
    cmd_add_option(cmd, OPTION_FLAG, "-o");
    cmd_add_output_file_option(cmd, obj);
    if (b_generate_debug_info)
    {
        cmd_add_option(cmd, OPTION_FLAG, "-g");
    }
    cmd_add_option(cmd, OPTION_FLAG, "-c");
    cmd_add_input_file_option(cmd, pcm);
    StringPtrHash map = {.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cc, &map);
    for (uint32_t i = map.begin; i != map.end; i = hash_next(&map, i))
    {
        char const* name = hash_key(&map, i);
        Node* bmi = hash_value(&map, i);
        if (!bmi)
        {
            continue;
        }
        char const* option = fmt("-fmodule-file={}=", name);
        cmd_add_option(cmd, OPTION_FLAG, option);
        cmd_add_input_file_option_no_sep(cmd, bmi);
        cmd_add_input(cmd, bmi);
    }
    cmd->prepare(cmd);
    return cmd;
}

void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd)
{
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        expect(cmd->export_bmi, "export BMI is NULL");
        cmd_add_output(node, cmd->export_bmi);
    }
    cmd_add_output(node, cmd->out_obj);
    if (cmd->scan_deps_cmd == NULL && cmd->b_cache_header_dependencies)
    {
        cmd_set_out_depfile(node, FILE("{}.d", cmd->out_obj->path));
    }
    if (cmd->export_bmi)
    {
        cmd->before_execute = compile_cmd_before_execute_rename_occupied_cc_file;
    }
}

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_input_file_option(node, cmd->src);
    if (cmd->arch)
    {
        cmd_add_option(node, OPTION_FLAG, get_arch_option_clang_or_gcc(cmd->arch));
    }
    if (cmd->optimization_type)
    {
        cmd_add_option(node, OPTION_FLAG, get_optimization_option_clang_or_gcc(cmd->optimization_type));
    }
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, OPTION_FLAG, "-g");
    }
    if (cmd->b_cpp)
    {
        char const* cpp_std_option = get_cpp_std_option_clang_or_gcc(cmd->cpp_std);
        if (cpp_std_option)
        {
            cmd_add_option(node, OPTION_FLAG, cpp_std_option);
        }
    }
    else
    {
        char const* c_std_option = get_c_std_option_clang_or_gcc(cmd->c_std);
        if (c_std_option)
        {
            cmd_add_option(node, OPTION_FLAG, c_std_option);
        }
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
}

void compile_cmdline_node_make_cmdline_llvm_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, OPTION_HIDDEN, "-fcolor-diagnostics");
        cmd_add_option(node, OPTION_HIDDEN, "-fansi-escape-codes");
    }
}

void compile_cmdline_node_make_cmdline_llvm_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, OPTION_EXE, get_clang_c_compiler());
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
}

void compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* module_name = hash_key(h, i);
        Node* bmi = hash_value(h, i);
        char const* option = fmt("-fmodule-file={}=", module_name);
        cmd_add_option(node, OPTION_FLAG, option);
        cmd_add_input_file_option_no_sep(node, bmi);
    }
}

static void compile_cmdline_node_make_cmdline_llvm_cpp_module(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, OPTION_EXE, get_clang_cpp_compiler());
    cmd_add_option(node, OPTION_FLAG, "-o");
    cmd_add_output_file_option(node, cmd->out_obj);
    cmd_add_option(node, OPTION_FLAG, "-c");
    cmd_add_option(node, OPTION_FLAG, "-fmodule-output=");
    cmd_add_output_file_option_no_sep(node, cmd->export_bmi);
    if (!is_clang_supported_module_extension(cmd->src->path))
    {
        cmd_add_option(node, OPTION_FLAG, "-x c++-module");
    }
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_llvm_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, OPTION_EXE, get_clang_cpp_compiler());
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_llvm_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, OPTION_EXE, get_clang_c_compiler());
    cmd_add_option(node, OPTION_FLAG, "-o");
    cmd_add_output_file_option(node, cmd->out_obj);
    cmd_add_option(node, OPTION_FLAG, "-c");
    cmd_add_input_file_option(node, cmd->src);
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, OPTION_FLAG, "-g");
    }
    if (cmd->arch)
    {
        cmd_add_option(node, OPTION_FLAG, get_arch_option_clang_or_gcc(cmd->arch));
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    if (!b_pure_asm && cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_llvm_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_llvm_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_llvm_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_llvm_asm(compile_cmdline);
    }
    else
    {
        fatal("unreachable");
    }
}
