#include "cup/c_toolchain/link_cmd.h"

#include "core/hash.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "cup/c_toolchain/ar_cmd.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include "core/macros.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

extern Allocator* node_allocator;
extern bool b_generate_debug_info;
extern ToolchainType default_toolchain;
extern ArchitectureType default_architecture_type;
extern OptimizationType default_optimization_type;
extern char* g_zig_target;
typedef struct LibOrderPair
{
    char const* lib;
    size_t index;
} LibOrderPair;

static int lib_order_pair_compare(void const* key, const void* element)
{
    LibOrderPair* k = (LibOrderPair*)key;
    LibOrderPair* e = (LibOrderPair*)element;
    if (k->index < e->index) return -1;
    else if (k->index == e->index) return 0;
    else return 1;
}

static void msvc_lib_output_filter(Node* cmd, char const* line)
{
    if (string_starts_with(line, "   Creating library"))
    {
        return;
    }
    cmd_write_stdout_line(cmd, line);
}

static Node* get_module_obj_from_bmi(Node* bmi)
{
    if (!bmi || !bmi->build_cmd)
    {
        return NULL;
    }

    if (bmi->build_cmd->cmd_ext_type == C_CMD_COMPILE)
    {
        return ((CCompileCmd*)bmi->build_cmd)->out_obj;
    }
    else if (bmi->build_cmd->cmd_ext_type == C_CMD_BMI_TO_OBJ)
    {
        return ((BmiToObjCmd*)bmi->build_cmd)->c_compile_cmd->out_obj;
    }

    return NULL;
}

static Node** collect_linked_node_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set);

static Node** collect_cpp_module_objs_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set)
{
    CCompileCmd* cc = obj_get_compile_cmd(input);
    if (!cc || !cc->b_cpp)
    {
        return nodes;
    }

    StringPtrHash h = {.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cc, &h);

    for (uint32_t i = h.begin; i != h.end; i = hash_next(&h, i))
    {
        Node* bmi = hash_value(&h, i);
        Node* module_obj = get_module_obj_from_bmi(bmi);

        if (module_obj)
        {
            nodes = collect_linked_node_recursively(allocator, module_obj, nodes, node_set, lib_set);
        }
    }
    return nodes;
}

static Node** collect_linked_node_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set)
{
    bool b_existed;
    hash_insert_check(node_set, input->path, &b_existed);
    if (b_existed)
    {
        return nodes;
    }
    array_push(allocator, nodes, input);
    if (input->file_type == FILE_TYPE_OBJ)
    {
        Obj* obj = (Obj*)input;
        for (size_t i = 0; i != array_size(obj->link_libs); i++)
        {
            uint32_t j = hash_insert_check(lib_set, obj->link_libs[i], &b_existed);
            if (!b_existed)
            {
                hash_value(lib_set, j) = lib_set->size - 1;
            }
        }
        for (size_t i = 0; i != array_size(obj->link_nodes); i++)
        {
            Node* other = obj->link_nodes[i];
            nodes = collect_linked_node_recursively(allocator, other, nodes, node_set, lib_set);
        }
        nodes = collect_cpp_module_objs_recursively(allocator, input, nodes, node_set, lib_set);
    }
    if (input->file_type == FILE_TYPE_LIB)
    {
        Node* lib = input;
        if (lib->build_cmd == NULL)
        {
            return nodes;
        }
        ArCmd* ar_cmd = (ArCmd*)lib->build_cmd;
        for (size_t i = 0; i != array_size(ar_cmd->inputs); i++)
        {
            Node* ar_input = ar_cmd->inputs[i];
            if (ar_input->file_type != FILE_TYPE_OBJ && ar_input->file_type != FILE_TYPE_LIB)
            {
                continue;
            }
            nodes = collect_linked_node_recursively(allocator, ar_input, nodes, node_set, lib_set);
        }
    }
    return nodes;
}

static void link_cmd_get_lib_obj_set(Node** files, Set* set)
{
    for (size_t i = 0; i != array_size(files); i++)
    {
        Node* input = files[i];
        if (input->file_type != FILE_TYPE_LIB)
        {
            continue;
        }
        if (input->build_cmd == NULL || input->build_cmd->cmd_ext_type != C_CMD_AR)
        {
            continue;
        }
        node_ensure_prepared(input);
        node_ensure_prepared(input->build_cmd);
        ArCmd* ar_cmd = (ArCmd*)input->build_cmd;
        for (size_t j = 0; j != array_size(ar_cmd->inputs); j++)
        {
            if (ar_cmd->inputs[j]->file_type == FILE_TYPE_OBJ)
            {
                hash_insert(set, (uintptr_t)ar_cmd->inputs[j]);
            }
        }
    }
}

static Node** link_cmd_collect_all_linked(LinkCmd* cmd, Allocator* temp_allocator, StringSet* node_set, StringHash* lib_set)
{
    Node** temp_files = NULL;
    for (size_t i = 0; i != array_size(cmd->input_option_files); i++)
    {
        Node* input = cmd->inputs[i];
        if (input->build_cmd == NULL)
        {
            continue;
        }
        node_ensure_prepared(input->build_cmd);
        temp_files = collect_linked_node_recursively(temp_allocator, input, temp_files, node_set, lib_set);
    }
    Set lib_objs = {.allocator = temp_allocator};
    for (size_t i = 0; i != array_size(temp_files); i++)
    {
        link_cmd_get_lib_obj_set(temp_files, &lib_objs);
    }
    Node** files = NULL;
    for (size_t i = 0; i != array_size(temp_files); i++)
    {
        Node* file = temp_files[i];
        if (!hash_has_key(&lib_objs, (uintptr_t)file))
        {
            array_push(temp_allocator, files, file);
        }
    }
    array_free(temp_allocator, temp_files);
    for (size_t i = 0; i != array_size(cmd->libs); i++)
    {
        bool b_existed;
        uint32_t j = hash_insert_check(lib_set, cmd->libs[i], &b_existed);
        if (!b_existed)
        {
            hash_value(lib_set, j) = lib_set->size - 1;
        }
    }
    return files;
}

static void link_cmd_add_link_lib_option(Node* node, char const* lib_name)
{
    LinkCmd* link = (LinkCmd*)node;
    extern char const* desc_color_input;
    extern char const* desc_color_reset;
    extern char const* desc_color_flag;

    if (link->linker_type == LINKER_LINK)
    {
        string_printf(node_allocator, node->cmdline, " %s.lib", lib_name);
        string_printf(node_allocator, node->description, " %s%s.lib%s", desc_color_flag, lib_name, desc_color_reset);
    }
    else
    {
        string_printf(node_allocator, node->cmdline, " -l%s", lib_name);
        string_printf(node_allocator, node->description, " %s-l%s%s%s%s", desc_color_flag, desc_color_reset, desc_color_input, lib_name, desc_color_reset);
    }
}
void link_cmd_add_input_options(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    Allocator* allocator = allocator_temp();
    StringSet node_set = {.allocator = allocator};
    StringHash lib_set = {.allocator = allocator};
    Node** all_inputs = link_cmd_collect_all_linked(link, allocator, &node_set, &lib_set);
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        if (input->file_type == FILE_TYPE_LIB)
        {
            continue;
        }
        cmd_add_input_file_option(cmd, input);
    }
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        if (input->file_type != FILE_TYPE_LIB)
        {
            continue;
        }
        cmd_add_input_file_option(cmd, input);
    }
    LibOrderPair* lib_order_pairs = NULL;
    for (uint32_t i = lib_set.begin; i != lib_set.end; i = hash_next(&lib_set, i))
    {
        char const* lib = hash_key(&lib_set, i);
        size_t index = hash_value(&lib_set, i);
        LibOrderPair p = {lib, index};
        array_push(allocator, lib_order_pairs, p);
    }
    if (lib_order_pairs)
    {
        size_t num_libs = array_size(lib_order_pairs);
        qsort(lib_order_pairs, num_libs, sizeof(LibOrderPair), lib_order_pair_compare);
    }
    for (size_t i = 0; i != array_size(lib_order_pairs); i++)
    {
        char const* lib = lib_order_pairs[i].lib;
        link_cmd_add_link_lib_option(cmd, lib);
    }
}

void link_cmd_add_all_linked_input(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    Allocator* allocator = allocator_temp();
    StringSet node_set = {.allocator = allocator};
    StringHash lib_set = {.allocator = allocator};
    Node** all_inputs = link_cmd_collect_all_linked(link, allocator, &node_set, &lib_set);
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        cmd_add_input(cmd, input);
    }
}

static bool link_cmd_can_output_pdb(LinkerType linker)
{
    if (linker == LINKER_LINK || linker == LINKER_LLVM_LINK)
    {
        return true;
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (linker == LINKER_LLVM_LD || linker == LINKER_LLVM_LLD))
    {
        return true;
    }
    return false;
}

Node* link_cmd_set_pdb_base_on_output(Node* node)
{
    LinkCmd* cmd = (LinkCmd*)node;
    if (!link_cmd_can_output_pdb(cmd->linker_type))
    {
        return NULL;
    }
#if CURRENT_PLATFORM != PLATFORM_WINDOWS
    return NULL;
#endif
    if (cmd->toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return NULL;
    }
    char const* pdb_path = fmt("{:n}.pdb", cmd->output);
    Node* pdb = get_or_add_file(pdb_path);
    link_cmd_set_pdb(node, pdb);
    return pdb;
}

static char const* link_cmd_get_out_option(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/out:";
    }
    if (linker_type == LINKER_LLVM_LINK || (CURRENT_PLATFORM == PLATFORM_WINDOWS && linker_type == LINKER_LLVM_LLD))
    {
        return "-Wl,/out:";
    }
    return "-o ";
}

static bool link_cmd_check_is_linking_cpp(LinkCmd* cmd)
{
    bool b_link_cpp_obj = false;
    for (size_t i = 0; i != array_size(cmd->inputs); i++)
    {
        Node* input = cmd->inputs[i];
        if (input->file_type != FILE_TYPE_OBJ)
        {
            continue;
        }
        if (!input->build_cmd || input->build_cmd->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        CCompileCmd* compile_cmd = (CCompileCmd*)input->build_cmd;
        if (compile_cmd->b_cpp)
        {
            b_link_cpp_obj = true;
            break;
        }
    }
    return b_link_cpp_obj;
}

static char const* link_cmd_get_linker_gcc_llvm_zig(LinkCmd* cmd, ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_GCC)
    {
        if (cmd->b_link_cpp)
        {
            return "g++";
        }
        else
        {
            return "gcc";
        }
    }
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM)
    {
        if (cmd->b_link_cpp)
        {
            return get_clang_cpp_compiler();
        }
        else
        {
            return get_clang_c_compiler();
        }
    }
    if (toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        if (cmd->b_link_cpp)
        {
            return "zig c++";
        }
        else
        {
            return "zig cc";
        }
    }
    fatal("unreachable");
    return NULL;
}

static char const* link_cmd_get_linker(LinkCmd* cmd)
{
    if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (cmd->linker_type == LINKER_LINK)
        {
            return "link";
        }
        if (cmd->linker_type == LINKER_LLVM_LD || cmd->linker_type == LINKER_LLVM_LLD || cmd->linker_type == LINKER_LLVM_LINK)
        {
            return link_cmd_get_linker_gcc_llvm_zig(cmd, cmd->toolchain);
        }
        fatal("Unknown linker");
    }
    else if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return "link";
    }
    else if (cmd->toolchain == TOOLCHAIN_TYPE_GCC || cmd->toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        return link_cmd_get_linker_gcc_llvm_zig(cmd, cmd->toolchain);
    }
    fatal("unreachable");
    return NULL;
}

static char const* link_cmd_get_option_shared(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/dll";
    }
    return "-shared";
}

static char const* link_cmd_get_option_pdb(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/pdb:";
    }
    if (linker_type == LINKER_LLVM_LINK || (CURRENT_PLATFORM == PLATFORM_WINDOWS && linker_type == LINKER_LLVM_LLD))
    {
        return "-Wl,/pdb:";
    }
    return NULL;
}

static char const* link_cmd_get_option_def(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/def:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (linker_type == LINKER_LD || linker_type == LINKER_ZIG_CC))
    {
        return "";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return "-Wl,/def:";
    }
    return NULL;
}

static char const* link_cmd_get_option_out_import_lib(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/implib:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (linker_type == LINKER_LLVM_LLD || linker_type == LINKER_LLVM_LINK)
        {
            return "-Wl,/implib:";
        }
        return "--out-implib ";
    }
    else
    {
        return NULL;
    }
}

static char const* link_cmd_get_option_entry(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/entry:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (linker_type == LINKER_LLVM_LLD || linker_type == LINKER_LLVM_LINK))
    {
        return "-Wl,/entry:";
    }
    if (linker_type == LINKER_LLVM_LD)
    {
        return "-Wl,-e ";
    }
    return "-e ";
}

static char* link_cmd_get_default_options_msvc_llvm_common(LinkCmd* link, Allocator* allocator, char* options)
{
    if (link->out_import_lib == NULL)
    {
        string_concat_c_str(allocator, options, " /noimplib");
    }
    return options;
}

static char const* link_cmd_get_default_options_llvm(LinkCmd* link, Allocator* allocator)
{
    char* options = NULL;
    if (link->b_link_cpp && CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        string_concat_c_str(allocator, options, " -stdlib=libc++");
    }
    if (link->linker_type == LINKER_LLVM_LD)
    {
        string_concat_c_str(allocator, options, " -fuse-ld=ld");
    }
    if (link->linker_type == LINKER_LLVM_LINK)
    {
        string_concat_c_str(allocator, options, " -fuse-ld=link");
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (link->linker_type == LINKER_LLVM_LLD || link->linker_type == LINKER_LLVM_LINK))
    {
        if (link->out_import_lib == NULL)
        {
            string_concat_c_str(allocator, options, " -Wl,/noimplib");
        }
    }
    return options;
}

static char const* link_cmd_get_default_options_msvc(LinkCmd* link, Allocator* allocator)
{
    char* options = NULL;
    options = link_cmd_get_default_options_msvc_llvm_common(link, allocator, options);
    string_concat_c_str(allocator, options, " /nologo /incremental:no");
    string_concat_c_str(allocator, options, " /noexp");
    return options;
}

static char const* link_cmd_get_default_options(LinkCmd* link, Allocator* allocator)
{
    if (link->toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        return link_cmd_get_default_options_llvm(link, allocator);
    }
    if (link->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return link_cmd_get_default_options_msvc(link, allocator);
    }
    return NULL;
}

static char const* link_cmd_get_option_arch(ToolchainType toolchain, ArchitectureType arch)
{
    if (toolchain != TOOLCHAIN_TYPE_MSVC)
    {
        if (arch == ARCH_X64)
        {
            return "-m64";
        }
        if (arch == ARCH_X86)
        {
            return "-m32";
        }
    }
    return NULL;
}

static char const* link_cmd_get_option_debug(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/debug";
    }
    return "-g";
}

static char const* link_cmd_get_option_lib_dir(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/libpath:";
    }
    return "-L";
}

static char const* link_cmd_get_option_lib(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return NULL;
    }
    else
    {
        return "-l";
    }
}

static LinkerType link_cmd_get_default_linker_type(ToolchainType toolchain)
{
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        return get_llvm_linker_type();
    }
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return LINKER_LINK;
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return LINKER_LD;
    }
    if (toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        return LINKER_ZIG_CC;
    }
    return LINKER_UNSPECIFIED;
}

void link_cmd_make_cmdline(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    array_resize(node_allocator, link->cmdline, 0);
    array_resize(node_allocator, link->description, 0);
    char const* opt_out = link_cmd_get_out_option(link->linker_type);
    char const* linker = link_cmd_get_linker(link);
    cmd_add_option(cmd, OPTION_EXE, linker);
    cmd_add_option(cmd, OPTION_FLAG, opt_out);
    cmd_add_option_no_sep(cmd, OPTION_OUTPUT, link->output->path);
    if (link->output->file_type == FILE_TYPE_DLL)
    {
        char const* opt_shared = link_cmd_get_option_shared(link->linker_type);
        cmd_add_option(cmd, OPTION_FLAG, opt_shared);
    }
    if (link->def)
    {
        char const* opt_def = link_cmd_get_option_def(link->linker_type);
        if (opt_def)
        {
            cmd_add_option(cmd, OPTION_FLAG, opt_def);
            cmd_add_option_no_sep(cmd, OPTION_INPUT, link->def->path);
        }
    }
    link_cmd_add_input_options(cmd);
    if (link->pdb)
    {
        char const* opt_pdb = link_cmd_get_option_pdb(link->linker_type);
        if (opt_pdb)
        {
            cmd_add_option(cmd, OPTION_FLAG, opt_pdb);
            cmd_add_option_no_sep(cmd, OPTION_OUTPUT, link->pdb->path);
        }
    }
    char const* opt_out_import_lib = link_cmd_get_option_out_import_lib(link->linker_type);
    if (link->out_import_lib && opt_out_import_lib)
    {
        cmd_add_option(cmd, OPTION_FLAG, opt_out_import_lib);
        cmd_add_option_no_sep(cmd, OPTION_OUTPUT, link->out_import_lib->path);
    }
    if (link->entry)
    {
        char const* opt_entry = link_cmd_get_option_entry(link->linker_type);
        cmd_add_option(cmd, OPTION_FLAG, fmt("{}{}", opt_entry, link->entry));
    }
    char const* opt_default = link_cmd_get_default_options(link, allocator_temp());
    if (opt_default)
    {
        cmd_add_option(cmd, OPTION_FLAG, opt_default + 1);
    }
    if (link->toolchain != TOOLCHAIN_TYPE_MSVC)
    {
        char const* opt_arch = link_cmd_get_option_arch(link->toolchain, link->arch);
        if (opt_arch)
        {
            cmd_add_option(cmd, OPTION_FLAG, opt_arch);
        }
    }
    if (link->pdb)
    {
        char const* opt_debug = link_cmd_get_option_debug(link->linker_type);
        cmd_add_option(cmd, OPTION_FLAG, opt_debug);
    }
    for (size_t i = 0; i != array_size(link->flags); i++)
    {
        cmd_add_option(cmd, OPTION_FLAG, link->flags[i]);
    }
    char const* opt_lib_dir = link_cmd_get_option_lib_dir(link->linker_type);
    for (size_t i = 0; i != array_size(link->lib_directories); i++)
    {
        cmd_add_option(cmd, OPTION_FLAG, fmt("{}{}", opt_lib_dir, link->lib_directories[i]));
    }

    if (link->toolchain == TOOLCHAIN_TYPE_ZIG && g_zig_target)
    {
        cmd_add_option(cmd, OPTION_FLAG, fmt("-target {}", g_zig_target));
    }
}

static void link_cmd_prepare(Node* node)
{
    LinkCmd* link = (LinkCmd*)node;
    link->b_link_cpp = link_cmd_check_is_linking_cpp(link);
    if (b_generate_debug_info)
    {
        if (link->pdb == NULL)
        {
            link_cmd_set_pdb_base_on_output(node);
        }
        if (link->pdb)
        {
            cmd_add_output(node, link->pdb);
        }
    }
    if (link->def)
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            cmd_add_input(node, link->def);
        }
    }
    if (link->b_auto_gen_out_import_lib)
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            char const* path = path_replace_extension(link->output->path, LIB_EXT, allocator_temp());
            link->out_import_lib = get_or_add_file(path);
        }
    }
    Node* env = get_toolchain_env_node(link->toolchain, link->arch);
    if (env)
    {
        cmd_set_env(node, env);
    }
    link_cmd_add_all_linked_input(node);

    cmd_prepare(node);
}

static void link_cmd_before_execute(Node* node)
{
    void c_toolchain_rename_to_old(char const* path);

    LinkCmd* link = (LinkCmd*)node;
    if (!os_file_writable(link->output->path))
    {
        c_toolchain_rename_to_old(link->output->path);
    }
    if (link->pdb && !os_file_writable(link->pdb->path))
    {
        c_toolchain_rename_to_old(link->pdb->path);
    }
    cmd_before_execute(node);
}

static bool link_cmd_check_dirty(Node* node)
{
    link_cmd_make_cmdline(node);
    return cmd_check_dirty(node);
}

Node* link_cmd_create(Node* output, char const* file, int line)
{
    expect(output->build_cmd == NULL, "output already has a build command");
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_LINK);
    Node* cmd = node_create(node_type, fmt("link: {:n}", output), sizeof(LinkCmd));
    LinkCmd* link = (LinkCmd*)cmd;
    link->toolchain = default_toolchain;
    link->output = output;
    link->arch = default_architecture_type;
    link->prepare = link_cmd_prepare;
    link->before_execute = link_cmd_before_execute;
    link->check_dirty = link_cmd_check_dirty;
    link->linker_type = link_cmd_get_default_linker_type(link->toolchain);
    link->optimization = default_optimization_type;
    cmd_set_source_location(cmd, file, line);
    cmd_add_output(cmd, output);
    node_set_extra_data(cmd, link);

    return cmd;
}

void link_cmd_add_input(Node* cmd, Node* file)
{
    LinkCmd* link_cmd = (LinkCmd*)cmd;
    array_push(node_allocator, link_cmd->input_option_files, file);
    cmd_add_input(cmd, file);
}

void link_cmd_set_pdb(Node* cmd, Node* pdb)
{
    LinkCmd* link = (LinkCmd*)cmd;
    if (!link_cmd_can_output_pdb(link->linker_type))
    {
        return;
    }
    link->pdb = pdb;
}

void link_cmd_set_out_import_lib(Node* cmd, Node* out_import_lib)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->out_import_lib = out_import_lib;
}

Node* link_cmd_get_out_import_lib(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    return link->out_import_lib;
}

void link_cmd_set_def_file(Node* cmd, Node* def)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    LinkCmd* link = (LinkCmd*)cmd;
    link->def = def;
}

void link_cmd_add_lib(Node* cmd, char const* lib)
{
    LinkCmd* link = (LinkCmd*)cmd;
    lib = string_from_c_str(node_allocator, lib);
    array_push(node_allocator, link->libs, lib);
}

void link_cmd_add_lib_dir(Node* cmd, char const* dir)
{
    LinkCmd* link = (LinkCmd*)cmd;
    dir = string_from_c_str(node_allocator, dir);
    array_push(node_allocator, link->lib_directories, dir);
}

void link_cmd_add_flag(Node* cmd, char const* flag)
{
    LinkCmd* link = (LinkCmd*)cmd;
    flag = string_from_c_str(node_allocator, flag);
    array_push(node_allocator, link->flags, flag);
}

void link_cmd_set_entry(Node* cmd, char const* name)
{
    LinkCmd* link = (LinkCmd*)cmd;
    array_resize(node_allocator, link->entry, 0);
    string_concat_c_str(node_allocator, link->entry, name);
}

void link_cmd_set_arch(Node* cmd, ArchitectureType arch)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->arch = arch;
}

void link_cmd_set_toolchain_type(Node* cmd, ToolchainType toolchain_type)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->toolchain = toolchain_type;
}

void link_cmd_set_linker_type(Node* cmd, LinkerType linker_type)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->linker_type = linker_type;
}

void link_cmd_setup_self_build(Node* cmd)
{
    extern ToolchainType self_build_toolchain;
    link_cmd_set_toolchain_type(cmd, self_build_toolchain);
    link_cmd_set_linker_type(cmd, link_cmd_get_default_linker_type(self_build_toolchain));
}
