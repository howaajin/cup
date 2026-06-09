#include "core/hash.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include "core/macros.h"
#include <assert.h>

extern Allocator* node_allocator;

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

static char const* get_optimization_option_cl(OptimizationType optimization)
{
    switch (optimization)
    {
    case OPTIMIZATION_TYPE_DEBUG: return "/Od";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "/Ox";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "/Os";
    case OPTIMIZATION_TYPE_UNSPECIFIED: return NULL;
    default: fatal("unknown optimization type"); return NULL;
    }
}

static char const* get_cpp_std_option_cl(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case CPP_LANGUAGE_STANDARD_98: return "/std:c++98";
    case CPP_LANGUAGE_STANDARD_11: return "/std:c++11";
    case CPP_LANGUAGE_STANDARD_14: return "/std:c++14";
    case CPP_LANGUAGE_STANDARD_17: return "/std:c++17";
    case CPP_LANGUAGE_STANDARD_20: return "/std:c++20";
    case CPP_LANGUAGE_STANDARD_23: return "/std:c++latest";
    case CPP_LANGUAGE_STANDARD_26: return "/std:c++latest";
    default: fatal("unknown C++ language standard"); return NULL;
    }
}

static char const* get_c_std_option_cl(CLanguageStandard c_std)
{
    switch (c_std)
    {
    case C_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case C_LANGUAGE_STANDARD_99: return "/std:c11";
    case C_LANGUAGE_STANDARD_11: return "/std:c11";
    case C_LANGUAGE_STANDARD_17: return "/std:c17";
    default: return "/std:clatest";
    }
}

void c_compile_cmd_write_buffer_msvc(Node* node, char const* line)
{
    CCompileCmd* cmd = (CCompileCmd*)node->ctx;
    char const* cwd = cmd->cwd;
    if (cmd->b_cache_header_dependencies)
    {
        if (strncmp(line, cmd->msvc_show_include_prefix, cmd->show_include_prefix_len) == 0)
        {
            char* dep = (char*)line + cmd->show_include_prefix_len;
            while (isspace(*dep)) dep += 1;
            if (path_is_absolute(dep))
            {
                if (path_is_under_directory(dep, cwd))
                {
                    Allocator* temp_allocator = allocator_temp();
                    dep = path_lexically_relative(dep, cwd, temp_allocator);
                    path_backslash_to_slash(dep);
                }
                else
                {
                    dep = NULL;
                }
            }
            if (dep)
            {
                cmd_add_implicit_input(node, dep);
            }
            return;
        }
    }
    if (string_equal(line, cmd->input_filename))
    {
        return;
    }
    cmd_write_stderr_line(node, line);
}

void c_compile_cmd_init_msvc(CCompileCmd* cmd)
{
    if (cmd->input_filename == NULL)
    {
        extern char* msvc_show_include_prefix;
        cmd->input_filename = path_filename(cmd->src->path, node_allocator);
        cmd->cwd = os_get_cwd(node_allocator);
        cmd->msvc_show_include_prefix = msvc_show_include_prefix;
        cmd->show_include_prefix_len = strlen(cmd->msvc_show_include_prefix);
    }
}

void c_compile_cmd_prepare_msvc(Node* node, CCompileCmd* cmd)
{
    c_compile_cmd_init_msvc(cmd);
    Node* env = msvc_get_env_node(cmd->toolchain, cmd->arch);
    cmd_set_env(node, env);
    cmd_add_output(node, cmd->out_obj);
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        expect(cmd->export_bmi, "export BMI is NULL");
        cmd_add_output(node, cmd->export_bmi);
    }
    if (cmd->b_generate_debug_info && cmd->source_type != SOURCE_TYPE_ASM)
    {
        if (cmd->pdb == NULL)
        {
            cmd->pdb = get_or_add_file(fmt("{:n}.pdb", cmd->out_obj));
        }
        cmd_add_output(node, cmd->pdb);
    }
    cmd->ctx = cmd;
    cmd->write_stdout_line_fn = c_compile_cmd_write_buffer_msvc;
    cmd->write_stderr_line_fn = c_compile_cmd_write_buffer_msvc;
}

static bool is_cl_supported_module_extension(char const* path)
{
    Allocator* allocator = allocator_temp();
    char* ext = (char*)path_extension(path);
    ext = string_from_c_str(allocator, ext);
    string_tolower(ext);
    if (string_equal(ext, ".ixx"))
    {
        return true;
    }
    return false;
}

void compile_cmdline_node_make_cmdline_msvc_scan_deps_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, OPTION_FLAG, "/nologo");
    if (cmd->b_cpp)
    {
        if (!is_cl_supported_module_extension(cmd->src->path))
        {
            cmd_add_option(node, OPTION_FLAG, "/TP");
        }
        if (cmd->export_name || cmd->export_bmi)
        {
            cmd_add_option(node, OPTION_FLAG, "/interface");
        }
    }
    if (array_size(cmd->import_names) || cmd->export_name)
    {
        cmd_add_option(node, OPTION_FLAG, "/EHsc");
    }
    cmd_add_input_file_option(node, cmd->src);
    if (cmd->optimization_type)
    {
        cmd_add_option(node, OPTION_FLAG, get_optimization_option_cl(cmd->optimization_type));
    }
    if (cmd->b_cpp)
    {
        char const* cpp_std_option = get_cpp_std_option_cl(cmd->cpp_std);
        if (cpp_std_option)
        {
            cmd_add_option(node, OPTION_FLAG, cpp_std_option);
        }
    }
    else
    {
        char const* c_std_option = get_c_std_option_cl(cmd->c_std);
        if (c_std_option)
        {
            cmd_add_option(node, OPTION_FLAG, c_std_option);
        }
    }
    compile_cmdline_node_append_string_set_options(node, "/I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "/D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
}

void compile_cmdline_node_make_cmdline_msvc_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, OPTION_EXE, "cl");
    cmd_add_option(node, OPTION_FLAG, "/Fo:");
    cmd_add_output_file_option_no_sep(node, cmd->out_obj);
    cmd_add_option(node, OPTION_FLAG, "/c");
    compile_cmdline_node_make_cmdline_msvc_scan_deps_common(node, cmd);
    if (cmd->b_generate_debug_info)
    {
        expect(cmd->pdb, "PDB path is NULL");
        cmd_add_option(node, OPTION_FLAG, "/Zi");
        cmd_add_option(node, OPTION_FLAG, "/Fd");
        cmd_add_output_file_option_no_sep(node, cmd->pdb);
    }
}

static void compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* module_name = hash_key(h, i);
        Node* bmi = hash_value(h, i);
        if (file_path_has_space(bmi))
        {
            char const* option = fmt("/reference \"{}=", module_name);
            cmd_add_option(node, OPTION_FLAG, option);
            cmd_add_option_no_sep(node, OPTION_INPUT, bmi->path);
            cmd_add_option_no_sep(node, OPTION_NONE, "\"");
        }
        else
        {
            char const* option = fmt("/reference {}=", module_name);
            cmd_add_option(node, OPTION_FLAG, option);
            cmd_add_input_file_option_no_sep(node, bmi);
        }
    }
}

static void compile_cmdline_node_make_cmdline_msvc_cppm(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    cmd_add_option(node, OPTION_FLAG, "/ifcOutput");
    cmd_add_output_file_option(node, cmd->export_bmi);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, OPTION_HIDDEN, "/showIncludes");
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_msvc_c_cpp(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, OPTION_HIDDEN, "/showIncludes");
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_msvc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    Allocator* temp = allocator_temp();
    char const* ext_old = path_extension(cmd->src->path);
    char* ext = string_from_c_str(temp, ext_old);
    string_tolower(ext);
    if (string_equal(ext, ".asm"))
    {
        cmd_add_option(node, OPTION_EXE, cmd->arch == ARCH_X64 ? "ml64" : "ml");
        cmd_add_option(node, OPTION_FLAG, "/nologo");
        cmd_add_option(node, OPTION_FLAG, "/quiet");
        cmd_add_option(node, OPTION_FLAG, "/c");
        if (cmd->b_generate_debug_info)
        {
            cmd_add_option(node, OPTION_FLAG, "/Zi");
        }
        cmd_add_option(node, OPTION_FLAG, "/Fo");
        cmd_add_option_no_sep(node, OPTION_OUTPUT, cmd->out_obj->path);
        cmd_add_output(node, cmd->out_obj);
        cmd_add_input_file_option(node, cmd->src);
        compile_cmdline_node_append_string_set_options(node, "/I", cmd->includes, OPTION_BRIGHT_FLAG);
        compile_cmdline_node_append_string_set_options(node, "/D", cmd->defines, OPTION_FLAG);
        compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    }
    else
    {
        fatal("MSVC does not support .s/.S files; use .asm with MASM syntax");
    }
}

void compile_cmdline_node_make_cmdline_msvc(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_msvc_cppm(node, cmd);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_msvc_asm(compile_cmdline);
    }
    else
    {
        compile_cmdline_node_make_cmdline_msvc_c_cpp(node, cmd);
    }
}

static void set_filenames(Node** nodes, size_t size, char const** filenames)
{
    for (size_t i = 0; i != size; i++)
    {
        char const* path = nodes[i]->path;
        size_t len = strlen(path);
        char const* p = path + len - 1;
        while (p != path && *p != '/' && *p != '\\') --p;
        if (p != path)
        {
            p += 1;
        }
        filenames[i] = p;
    }
}
