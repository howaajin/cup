#include "cup/c_toolchain/c_toolchain.h"

#include "core/array.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "cup/c_toolchain/ar_cmd.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/cache.h"
#include "cup/cup.private.h"
#include "cup/fmt.h"
#include "cup/node.h"
#include "cup/var.h"
#include <stdio.h>

#include <assert.h>
#include <stdlib.h>

extern Allocator* node_allocator;

ToolchainType default_toolchain = TOOLCHAIN_TYPE_UNSPECIFIED;
ToolchainType self_build_toolchain = TOOLCHAIN_TYPE_UNSPECIFIED;
bool b_generate_debug_info = true;
bool b_test_enabled = false;
bool b_cache_header_dependencies = true;
ArchitectureType default_architecture_type = CURRENT_ARCHITECTURE;
OptimizationType default_optimization_type;
CLanguageStandard default_c_std;
CppLanguageStandard default_cpp_std;
char** build_script_search_directories;
StringSet* build_scripts = NULL;
char* msvc_show_include_prefix;
char* g_zig_target = NULL;
LinkerType linker_type = LINKER_UNSPECIFIED;
static bool b_linker_type_explicit = false;

static LinkerType c_toolchain_select_linker_automatically(ToolchainType toolchain)
{
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return LINKER_LINK;
    }
    else if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            return LINKER_LLVM_LLD;
        }
        else
        {
            return LINKER_LLVM_LD;
        }
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return LINKER_LD;
    }
    return LINKER_UNSPECIFIED;
}

static bool is_llvm_linker_type(LinkerType type)
{
    return type == LINKER_LLVM_LD || type == LINKER_LLVM_LLD || type == LINKER_LLVM_LINK;
}

void set_default_toolchain(ToolchainType type)
{
    default_toolchain = type;
    if (b_linker_type_explicit)
    {
        if (type != TOOLCHAIN_TYPE_LLVM)
        {
            error("LLVM linker selection requires the LLVM toolchain");
            exit(EXIT_FAILURE);
        }
        return;
    }
    linker_type = c_toolchain_select_linker_automatically(type);
}

ToolchainType get_default_toolchain(void)
{
    return default_toolchain;
}

void set_llvm_linker_type(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        error("set_llvm_linker_type only accepts LLVM -fuse-ld linker types");
        exit(EXIT_FAILURE);
    }
    if (default_toolchain != TOOLCHAIN_TYPE_UNSPECIFIED && default_toolchain != TOOLCHAIN_TYPE_LLVM)
    {
        return;
    }
    linker_type = type;
}

void c_toolchain_set_llvm_linker_type_explicit(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        error("c_toolchain_set_llvm_linker_type_explicit only accepts LLVM -fuse-ld linker types");
        exit(EXIT_FAILURE);
    }
    if (default_toolchain != TOOLCHAIN_TYPE_UNSPECIFIED && default_toolchain != TOOLCHAIN_TYPE_LLVM)
    {
        error("LLVM linker selection requires the LLVM toolchain");
        exit(EXIT_FAILURE);
    }
    linker_type = type;
    b_linker_type_explicit = true;
}

void c_toolchain_restore_llvm_linker_type(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        return;
    }
    linker_type = type;
}

LinkerType get_llvm_linker_type(void)
{
    if (!is_llvm_linker_type(linker_type))
    {
        return c_toolchain_select_linker_automatically(TOOLCHAIN_TYPE_LLVM);
    }
    return linker_type;
}

bool c_toolchain_is_linker_type_explicit(void)
{
    return b_linker_type_explicit;
}

void set_default_architecture(ArchitectureType type)
{
    default_architecture_type = type;
}

ArchitectureType get_default_architecture(void)
{
    return default_architecture_type;
}

void set_default_optimization(OptimizationType type)
{
    default_optimization_type = type;
}

OptimizationType get_default_optimization(void)
{
    return default_optimization_type;
}

void set_self_build_toolchain(ToolchainType toolchain)
{
    self_build_toolchain = toolchain;
}

ToolchainType get_toolchain_by_current_compiler()
{
    if (CURRENT_COMPILER == COMPILER_CLANG)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (CURRENT_COMPILER == COMPILER_CL)
    {
        return TOOLCHAIN_TYPE_MSVC;
    }
    if (CURRENT_COMPILER == COMPILER_GCC)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    return TOOLCHAIN_TYPE_UNSPECIFIED;
}

void set_debug_info_enabled(bool b_enabled)
{
    b_generate_debug_info = b_enabled;
}

void set_test_enabled(bool b_enabled)
{
    b_test_enabled = b_enabled;
}

void set_default_arch(ArchitectureType arch)
{
    default_architecture_type = arch;
}

void set_default_c_std(CLanguageStandard std)
{
    default_c_std = std;
}

CLanguageStandard get_default_c_std(void)
{
    return default_c_std;
}

void set_default_cpp_std(CppLanguageStandard std)
{
    default_cpp_std = std;
}

CppLanguageStandard get_default_cpp_std(void)
{
    return default_cpp_std;
}

void set_zig_target(char const* target)
{
    array_resize(allocator_c(), g_zig_target, 0);
    string_concat_c_str(allocator_c(), g_zig_target, target);
}

void set_msvc_show_include_prefix(char const* prefix)
{
    array_resize(allocator_c(), msvc_show_include_prefix, 0);
    string_concat_c_str(allocator_c(), msvc_show_include_prefix, prefix);
}

void add_build_script(char const* path)
{
    if (build_scripts == NULL)
    {
        build_scripts = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        build_scripts->allocator = node_allocator;
    }
    path = string_from_c_str(node_allocator, path);
    hash_insert(build_scripts, path);
}

void add_build_script_search_directory(char const* directory)
{
    char* dir = string_from_c_str(allocator_c(), directory);
    array_push(allocator_c(), build_script_search_directories, dir);
}

ToolchainType c_toolchain_select_toolchain_automatically();

static char const* c_toolchain_next_line(char const* str)
{
    char const* p = str;
    while (*p)
    {
        if (*p == '\r' && *(p + 1) == '\n')
        {
            p += 2;
            break;
        }
        if (*p == '\n') return p + 1;
        ++p;
    }
    return p;
}

char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
    char const* src_rel_path = src_path;
    if (path_is_absolute(src_path))
    {
        char const* cwd = os_get_cwd(temp_allocator);
        if (path_is_under_directory(src_path, cwd))
        {
            src_rel_path = path_lexically_relative(src_path, cwd, temp_allocator);
        }
        else
        {
            src_rel_path = path_windows_style_to_linux_relative(src_path, temp_allocator);
        }
    }
    char* obj_path;
    if (src_rel_path != src_path)
    {
        obj_path = fmt_alloc(allocator, "{out_dir}/{}/External/{}{}", sub_dir, src_rel_path, ext);
    }
    else
    {
        obj_path = fmt_alloc(allocator, "{out_dir}/{}/{}{}", sub_dir, src_rel_path, ext);
    }
    return obj_path;
}

Obj* obj_create(char const* path)
{
    uint32_t node_type = node_make_file_type(FILE_TYPE_OBJ, 0);
    Obj* obj = (Obj*)get_or_add_node(node_type, path, sizeof(Obj));
    return obj;
}

Node* obj_from_src(Node* src)
{
    Allocator* allocator = allocator_temp();
    char const* obj_dir = get_var("obj_dir");
    char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
    return (Node*)obj_create(obj_path);
}

Node* obj_from_src_with_variant(Node* src, char const* variant)
{
    Allocator* allocator = allocator_temp();
    char const* obj_dir = get_var("obj_dir");
    char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
    char const* stem = path_stem(obj_path, allocator_temp());
    char const* parent_path = path_parent_path(obj_path, allocator_temp());
    if (parent_path)
    {
        obj_path = fmt("{}/{}{}{}", parent_path, stem, variant, OBJ_EXT);
    }
    else
    {
        obj_path = fmt("{}{}{}", stem, variant, OBJ_EXT);
    }
    return (Node*)obj_create(obj_path);
}

Node* get_default_obj(Node* node)
{
    Src* src = (Src*)node;
    if (src->default_obj == NULL)
    {
        Allocator* allocator = allocator_temp();
        char const* obj_dir = get_var("obj_dir");
        char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
        src->default_obj = obj_create(obj_path);
    }
    return (Node*)src->default_obj;
}

void obj_add_link_obj_from_src(Node* node, Node* src)
{
    Node* obj = get_default_obj(src);
    obj_add_link_node(node, obj);
}

void obj_add_link_node(Node* node, Node* other)
{
    if (node->node_type != NODE_TYPE_FILE ||
        (node->file_type != FILE_TYPE_OBJ && node->file_type != FILE_TYPE_LIB))
    {
        assert(false && "An object link node can only be a .lib (library) or another .obj (object file).");
    }
    Obj* obj = (Obj*)node;
    array_push(node_allocator, obj->link_nodes, other);
}

void obj_add_link_lib(Node* node, char const* lib)
{
    Obj* obj = (Obj*)node;
    lib = string_from_c_str(node_allocator, lib);
    array_push(node_allocator, obj->link_libs, lib);
}

CCompileCmd* obj_get_compile_cmd(Node* obj)
{
    Node* cmd = obj->build_cmd;
    if (cmd == NULL)
    {
        return NULL;
    }
    CCompileCmd* cc = NULL;
    if (cmd->cmd_ext_type == C_CMD_COMPILE)
    {
        cc = (CCompileCmd*)cmd;
    }
    else if (cmd->cmd_ext_type == C_CMD_BMI_TO_OBJ)
    {
        BmiToObjCmd* cmd_bmi_to_obj = (BmiToObjCmd*)cmd;
        cc = cmd_bmi_to_obj->c_compile_cmd;
    }
    return cc;
}

char** obj_get_compile_includes(Node* obj, Allocator* allocator)
{
    char** includes = NULL;
    CCompileCmd* cc = obj_get_compile_cmd(obj);
    if (cc == NULL)
    {
        return NULL;
    }
    StringSet* s = cc->includes;
    for (uint32_t i = s->begin; i != s->end; i = hash_next(s, i))
    {
        char* inc = (char*)hash_key(s, i);
        array_push(allocator, includes, inc);
    }
    return includes;
}

bool is_test_exe(Node* exe)
{
    return exe->node_type == NODE_TYPE_FILE &&
           exe->file_type == FILE_TYPE_EXE &&
           exe->file_ext_type == C_FILE_TEST;
}

char const** get_test_entries(Node* exe)
{
    TestExe* test = (TestExe*)exe;
    return test->entries;
}

char const* get_architecture_string(ArchitectureType arch)
{
    if (arch == ARCH_X64)
    {
        return "x64";
    }
    else if (arch == ARCH_X86)
    {
        return "x86";
    }
    else if (arch == ARCH_ARM)
    {
        return "arm";
    }
    else if (arch == ARCH_ARM64)
    {
        return "arm64";
    }
    return NULL;
}

static char const* get_test_exe_path_for_src(Allocator* allocator, char const* src_path)
{
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return fmt_alloc(allocator, "{out_dir}/tests/{}" EXE_EXT, src_path);
    }
    else
    {
        return fmt_alloc(allocator, "{out_dir}/tests/{}.test", src_path);
    }
}

bool run_test_cmd_check_dirty(Node* node)
{
    return true;
}

void gen_test_src(void);

Node* add_test_exe_for_obj(Node* obj, char const** entries)
{
    Allocator* temp_allocator = allocator_temp();
    CCompileCmd* cc = (CCompileCmd*)obj->build_cmd;
    char const* path = get_test_exe_path_for_src(temp_allocator, cc->src->path);
    Node* exe = find_node(path);
    if (exe)
    {
        return exe;
    }
    uint32_t node_type = node_make_file_type(FILE_TYPE_EXE, C_FILE_TEST);
    exe = node_create(node_type, path, sizeof(TestExe));
    c_compile_cmd_add_define(obj->build_cmd, "BUILD_TEST");
    c_compile_cmd_add_include_directory(obj->build_cmd, get_var("test_src_dir"));
    Node* obj_test = OBJ(SRC("{test_src_dir}/cup/test.c"));
    obj_add_link_node(obj, obj_test);
    Node* test_h = FILE("{test_src_dir}/cup/test.h");
    node_add_dependency(obj->build_cmd, test_h);
    TestExe* test_exe = (TestExe*)exe;
    test_exe->entries = entries;
    Node* link = LINK(exe);
    cmd_set_source_location(link, obj->build_cmd->file, obj->build_cmd->line);
    link_cmd_add_input(link, obj);
    {
        Node* src = SRC("{test_src_dir}/cup/test_main.c");
        if (src->build_cmd == NULL)
        {
            gen_test_src();
        }
        Node* obj = OBJ(src);
        if (obj->build_cmd == NULL)
        {
            Node* cc = CC(src, obj);
            node_add_dependency(cc, test_h);
        }
        link_cmd_add_input(link, obj);
    }
    {
        Node* src = SRC("{test_src_dir}/cup/test.c");
        if (src->build_cmd == NULL)
        {
            gen_test_src();
        }
        Node* obj = OBJ(src);
        if (obj->build_cmd == NULL)
        {
            CC(src, obj);
        }
        link_cmd_add_input(link, obj);
    }

    return exe;
}

static void c_toolchain_lib_output_filter(Node* cmd, char const* line)
{
    if (string_starts_with(line, "   Creating library"))
    {
        return;
    }
    cmd_write_stdout_line(cmd, line);
}

Node* make_implib_from_def_cmd_create(Node* def, Node* output, ToolchainType toolchain_type, ArchitectureType architecture_type, char const* src_file_path, int line)
{
    Node* cmd = add_process_cmd(NULL, src_file_path, line);
    cmd->cmd_ext_type = C_CMD_MAKE_IMPLIB;
    cmd_set_source_location(cmd, src_file_path, line);
    Node* env = get_toolchain_env_node(toolchain_type, architecture_type);
    cmd_set_env(cmd, env);
    char const* lib_name = "llvm-lib";
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        lib_name = "lib";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        lib_name = "zig lib";
    }
    cmd_add_option(cmd, NULL, lib_name, OPTION_EXE);
    cmd_add_option(cmd, "/out:", output->path, OPTION_OUTPUT);
    cmd_add_option(cmd, "/def:", def->path, OPTION_INPUT);
    char const* arch;
    if (architecture_type == ARCH_X64)
    {
        arch = "x64";
    }
    else if (architecture_type == ARCH_X86)
    {
        arch = "x86";
    }
    else
    {
        error("Unsupported arch");
        exit(EXIT_FAILURE);
    }
    cmd_add_option(cmd, "/machine:", arch, OPTION_FLAG);
    cmd_add_input(cmd, def);
    cmd_add_output(cmd, output);
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        cmd_add_option(cmd, "/nologo", NULL, OPTION_HIDDEN);
        char const* exp_path = path_replace_extension(output->path, ".exp", allocator_temp());
        Node* exp = get_or_add_file(exp_path);
        cmd_add_output(cmd, exp);
        cmd_set_write_output_line_fn(cmd, c_toolchain_lib_output_filter);
    }
    return cmd;
}

void c_toolchain_rename_to_old(char const* path)
{
    Allocator* allocator = allocator_create_chained();
    if (path_is_absolute(path))
    {
        char const* cwd = get_var("workspace");
        if (!path_is_under_directory(path, cwd))
        {
            allocator_destroy(allocator);
            return;
        }
        else path = path_lexically_relative(path, cwd, allocator);
    }
    char* path_without_ext = path_replace_extension(path, "", allocator);
    char* old_path_base = fmt_alloc(allocator, "{out_dir}/old/{}", path_without_ext);
    char const* ext = path_extension(path);
    char* old_path = string_from_print(allocator, "%s%s", old_path_base, ext);
    int num = 0;
    FILE* try_file = NULL;
    while (true)
    {
        if (!os_file_exists(old_path)) break;
        try_file = os_fopen(old_path, "r+b");
        if (try_file != NULL) break;
        num++;
        array_resize(allocator, old_path, 0);
        string_printf(allocator, old_path, "%s.%d%s", old_path_base, num, ext);
    }
    if (try_file)
    {
        fclose(try_file);
    }
    os_ensure_dir_existed(old_path);
    if (!os_rename(path, old_path))
    {
        error("os_rename failed!");
        exit(EXIT_FAILURE);
    }
    allocator_destroy(allocator);
}

void init_toolchain(void)
{
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_default_toolchain(get_toolchain_by_current_compiler());
    }
    set_msvc_show_include_prefix("Note: including file:");
    compile_commands_json_path = "compile_commands.json";
    set_var("arch", get_architecture_string(default_architecture_type));
    char const* cfg = "release";
    if (default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED)
    {
        default_optimization_type = OPTIMIZATION_TYPE_DEBUG;
    }
    if (default_optimization_type == OPTIMIZATION_TYPE_DEBUG)
    {
        cfg = "debug";
    }
    set_var("cfg", cfg);
    set_var("test_src_dir", "src");
}

Node* get_or_add_src(char const* path)
{
    Node* node = find_node(path);
    if (!node)
    {
        uint32_t node_type = node_make_file_type(FILE_TYPE_SRC, 0);
        node = node_create(node_type, path, sizeof(Src));
    }
    return node;
}
