#include "core/hash.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/fmt.h"
#include "cup/node.h"

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
    default: assert(false); return NULL;
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
    default: assert(false); return NULL;
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
                    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
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
        assert(cmd->export_bmi);
        cmd_add_output(node, cmd->export_bmi);
    }
    if (cmd->b_generate_debug_info)
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
    cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
    if (cmd->b_cpp)
    {
        if (!is_cl_supported_module_extension(cmd->src->path))
        {
            cmd_add_option(node, "/TP", NULL, OPTION_FLAG);
        }
        if (cmd->export_name || cmd->export_bmi)
        {
            cmd_add_option(node, "/interface", NULL, OPTION_FLAG);
        }
    }
    if (array_size(cmd->import_names) || cmd->export_name)
    {
        cmd_add_option(node, "/EHsc", NULL, OPTION_FLAG);
    }
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->optimization_type)
    {
        cmd_add_option(node, get_optimization_option_cl(cmd->optimization_type), NULL, OPTION_FLAG);
    }
    if (cmd->b_cpp)
    {
        char const* cpp_std_option = get_cpp_std_option_cl(cmd->cpp_std);
        if (cpp_std_option)
        {
            cmd_add_option(node, cpp_std_option, NULL, OPTION_FLAG);
        }
    }
    else
    {
        char const* c_std_option = get_c_std_option_cl(cmd->c_std);
        if (c_std_option)
        {
            cmd_add_option(node, c_std_option, NULL, OPTION_FLAG);
        }
    }
    compile_cmdline_node_append_string_set_options(node, "/I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "/D", cmd->defines, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
}

void compile_cmdline_node_make_cmdline_msvc_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, NULL, "cl", OPTION_EXE);
    cmd_add_output_file_option(node, "/Fo:", cmd->out_obj);
    cmd_add_option(node, "/c", NULL, OPTION_FLAG);
    if (cmd->optimization_type == OPTIMIZATION_TYPE_DEBUG)
    {
        cmd_add_option(node, "/MDd", NULL, OPTION_FLAG);
    }
    else
    {
        cmd_add_option(node, "/MD", NULL, OPTION_FLAG);
    }
    compile_cmdline_node_make_cmdline_msvc_scan_deps_common(node, cmd);
    if (cmd->b_generate_debug_info)
    {
        assert(cmd->pdb);
        cmd_add_option(node, "/Zi", NULL, OPTION_FLAG);
        cmd_add_output_file_option(node, "/Fd", cmd->pdb);
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
            cmd_add_option(node, option, bmi->path, OPTION_INPUT);
            cmd_add_option(node, "\"", NULL, OPTION_NONE);
        }
        else
        {
            char const* option = fmt("/reference {}=", module_name);
            cmd_add_input_file_option(node, option, bmi);
        }
    }
}

static void compile_cmdline_node_make_cmdline_msvc_cppm(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    cmd_add_output_file_option(node, "/ifcOutput ", cmd->export_bmi);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, "/showIncludes", NULL, OPTION_HIDDEN);
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_msvc_c_cpp(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, "/showIncludes", NULL, OPTION_HIDDEN);
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

void compile_cmdline_node_make_cmdline_msvc(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_msvc_cppm(node, cmd);
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
