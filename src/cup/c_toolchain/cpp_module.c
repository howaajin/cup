#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include <assert.h>

extern ToolchainType default_toolchain;

char const* msvc_find_std_module_source(bool b_compat);
char const* get_toolchain_bmi_extension(ToolchainType toolchain_type);
char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator);

static char const* probe_module_source(Allocator* alloc, char** include_dirs, char const** suffixes, size_t suffix_count)
{
    for (size_t i = 0; i < array_size(include_dirs); i++)
    {
        for (size_t j = 0; j < suffix_count; j++)
        {
            char* candidate = string_from_print(alloc, "%s/%s", include_dirs[i], suffixes[j]);
            if (os_file_exists(candidate))
            {
                return candidate;
            }
        }
    }
    return NULL;
}

static char** get_compiler_include_dirs(Allocator* alloc, char const* compiler_exe)
{
    char* cmd = string_from_print(alloc, "echo | %s -E -Wp,-v -x c++ - 2>&1", compiler_exe);
    FILE* f = os_popen(cmd, "r");
    if (!f) return NULL;

    char** dirs = NULL;
    char buffer[4096];
    bool in_search_list = false;

    while (fgets(buffer, sizeof(buffer), f))
    {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[len - 1] = '\0';
            len--;
        }

        if (strstr(buffer, "#include <...> search starts here:"))
        {
            in_search_list = true;
            continue;
        }
        if (strstr(buffer, "End of search list."))
        {
            break;
        }

        if (in_search_list)
        {
            char* path = buffer;
            while (*path == ' ') path++;
            if (*path)
            {
                char* normal = path_lexically_normal(path, alloc);
                array_push(alloc, dirs, normal);
            }
        }
    }

    os_pclose(f);
    return dirs;
}

static char const* find_std_module_source(ToolchainType toolchain, bool b_compat)
{
    static char const* cached_std = NULL;
    static char const* cached_compat = NULL;

    char const** cache_ptr = b_compat ? &cached_compat : &cached_std;
    if (*cache_ptr) return *cache_ptr;

    Allocator* temp = allocator_temp();
    char const* result = NULL;

    if (toolchain == TOOLCHAIN_TYPE_MSVC || (CURRENT_PLATFORM == PLATFORM_WINDOWS && toolchain != TOOLCHAIN_TYPE_GCC))
    {
        result = msvc_find_std_module_source(b_compat);
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_LLVM || toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        char const* compiler = (toolchain == TOOLCHAIN_TYPE_GCC) ? "g++" : "clang++";
        char** include_dirs = get_compiler_include_dirs(temp, compiler);

        if (include_dirs)
        {
            char const* stdcpp_suffixes[] = {
                b_compat ? "bits/std.compat.cc" : "bits/std.cc",
            };
            result = probe_module_source(temp, include_dirs, stdcpp_suffixes, static_array_size(stdcpp_suffixes));

            if (!result)
            {
                char const* cpp_suffixes[] = {
                    b_compat ? "v1/std.compat.cppm" : "v1/std.cppm",
                    b_compat ? "../share/libc++/v1/std.compat.cppm" : "../share/libc++/v1/std.cppm",
                };
                ;
                result = probe_module_source(temp, include_dirs, cpp_suffixes, static_array_size(cpp_suffixes));
            }
        }
    }

    if (result)
    {
        *cache_ptr = string_from_c_str(allocator_c(), result);
    }
    return *cache_ptr;
}

static Node* get_std_module_source_node(ToolchainType toolchain_type)
{
    char const* path = find_std_module_source(toolchain_type, false);
    if (path)
    {
        return get_or_add_src(path);
    }
    else
    {
        return NULL;
    }
}

static Node* get_std_compat_module_source_node(ToolchainType toolchain_type)
{
    char const* path = find_std_module_source(toolchain_type, true);
    if (path)
    {
        return get_or_add_file(path);
    }
    else
    {
        return NULL;
    }
}

static char const* get_std_module_bmi_path(
    char const* source,
    ToolchainType toolchain_Type,
    ArchitectureType arch,
    OptimizationType optimization_type,
    CppLanguageStandard cpp_std)
{
    char const* filename = path_filename(source, allocator_temp());
    char const* toolchain_string = get_toolchain_string(toolchain_Type);
    char const* arch_string = get_arch_string(arch);
    char const* optimization_type_string = get_optimization_string(optimization_type);
    char const* std_string = get_cpp_std_string(cpp_std);
    char const* ext = get_toolchain_bmi_extension(toolchain_Type);
    char* path = (char*)fmt("{out_dir}/modules/std/{}", toolchain_string);
    if (arch_string)
    {
        string_printf(allocator_temp(), path, "/%s", arch_string);
    }
    if (optimization_type_string)
    {
        string_printf(allocator_temp(), path, "/%s", optimization_type_string);
    }
    if (std_string)
    {
        string_printf(allocator_temp(), path, "/%s", std_string);
    }

    string_printf(allocator_temp(), path, "/%s%s", filename, ext);
    return path;
}

static Node* obj_from_bmi(Node* bmi)
{
    return (Node*)obj_create(fmt("{:n}{obj_ext}", bmi));
}

static void add_compile_std_module_extra_options(Node* node, ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM)
    {
        c_compile_cmd_add_flag(node, "-Wno-reserved-module-identifier");
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            c_compile_cmd_add_flag(node, "-Wno-include-angled-in-module-purview");
        }
    }
}

Node* create_compile_std_module_for_compile_cmd(CCompileCmd* cmd, Node* src, Node* bmi, bool b_compat)
{
    Node* obj = obj_from_bmi(bmi);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);
    c_compile_cmd_set_export(node, b_compat ? "std.compat" : "std", bmi);
    add_compile_std_module_extra_options(node, cmd->toolchain);
    CCompileCmd* compile_std_cmd = (CCompileCmd*)node;
    compile_std_cmd->toolchain = cmd->toolchain;
    compile_std_cmd->cpp_std = cmd->cpp_std;
    compile_std_cmd->optimization_type = cmd->optimization_type;
    compile_std_cmd->arch = cmd->arch;
    return node;
}

Node* get_or_create_std_module_for_compile_cmd(CCompileCmd* cmd)
{
    Node* src = get_std_module_source_node(cmd->toolchain);
    if (src == NULL)
    {
        return NULL;
    }
    char const* bmi_path = get_std_module_bmi_path(src->path, cmd->toolchain, cmd->arch, cmd->optimization_type, cmd->cpp_std);
    Node* bmi = find_node(bmi_path);
    if (bmi)
    {
        return bmi;
    }
    bmi = get_or_add_file(bmi_path);
    create_compile_std_module_for_compile_cmd(cmd, src, bmi, false);
    return bmi;
}

Node* get_or_create_std_compat_module_for_compile_cmd(CCompileCmd* cmd)
{
    Node* src = get_std_compat_module_source_node(cmd->toolchain);
    char const* bmi_path = get_std_module_bmi_path(src->path, cmd->toolchain, cmd->arch, cmd->optimization_type, cmd->cpp_std);
    Node* bmi = find_node(bmi_path);
    if (bmi)
    {
        return bmi;
    }
    bmi = get_or_add_file(bmi_path);
    create_compile_std_module_for_compile_cmd(cmd, src, bmi, false);
    return bmi;
}

Node* module_from_src(Node* src)
{
    Allocator* temp_allocator = allocator_temp();
    char const* ext = get_toolchain_bmi_extension(default_toolchain);
    char const* bmi_path = determine_imtermediate_path(src->path, "modules", ext, temp_allocator);
    return get_or_add_file(bmi_path);
}

Node* module_from_src_with_variant(Node* src, char const* variant)
{
    Allocator* temp_allocator = allocator_temp();
    char const* ext = get_toolchain_bmi_extension(default_toolchain);
    char const* bmi_path = determine_imtermediate_path(src->path, "modules", ext, temp_allocator);
    char const* stem = path_stem(bmi_path, allocator_temp());
    char const* parent_path = path_parent_path(bmi_path, allocator_temp());
    if (parent_path)
    {
        bmi_path = fmt("{}/{}{}{}", parent_path, stem, variant, ext);
    }
    else
    {
        bmi_path = fmt("{}{}{}", stem, variant, ext);
    }
    return get_or_add_file(bmi_path);
}