#include "cup/cup.h"
#include "core/array.h"
#include "core/directory.h"
#include "core/dylib.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "core/utilities.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/c_toolchain/scan_deps_cmd.h"
#include "cup/cache.h"
#include "cup/entry.h"
#include "cup/executor/executor.h"
#include "cup/graph.h"
#include "cup/var.h"

#include <stdbool.h>
#include <stdio.h>

extern Node** nodes;
extern Allocator* node_allocator;
extern ToolchainType self_build_toolchain;
extern ToolchainType default_toolchain;
extern OptimizationType default_optimization_type;
extern bool b_test_enabled;
extern bool b_node_default_excluded;

bool b_compdb = false;
bool b_dll_mode = false;
bool b_clean = false;
bool b_dry_run = false;
bool b_run_tests = false;
bool b_print_exe_entries = false;
bool b_generate_vscode_files = false;
bool b_bootstrap = false;
bool b_scan_deps_enabled = true;
bool b_content_hash = true;

char const* cl_show_include_prefix = "";
size_t max_jobs;
char** target_names;
char const* cup_h_dir = ".";
char const* vscode_debugger_type = NULL;
char const* init_cwd = NULL;
Node** targets = NULL;
Dylib* cup_dll = NULL;
FnAfterPrepare* fn_after_prepare = NULL;

static LockFileContext* process_lock_ctx;
size_t max_build_errors = 1;

void init_cache(void);
void destroy_var(void);
void save_last_status(void);

void destroy(void)
{
    extern Cache* cache;
    if (cache)
    {
        save_last_status();
        cache_compact_log(cache, fmt("{out_dir}/.cup_cache"));
        cache_destroy(cache);
        cache = NULL;
    }
    if (process_lock_ctx)
    {
        os_unlock_file(process_lock_ctx);
        process_lock_ctx = NULL;
    }
    if (node_allocator)
    {
        allocator_destroy(node_allocator);
        node_allocator = NULL;
    }
    if (init_cwd)
    {
        os_set_cwd(init_cwd);
        array_free(allocator_c(), init_cwd);
        init_cwd = NULL;
    }
    destroy_var();
    if (cup_dll)
    {
        dylib_unload(cup_dll);
        cup_dll = NULL;
    }
}

// To support multi-file selection compilation in Visual Studio
static char** objects_from_sources_string(char* sources_sep_with_semicolon, char const* cwd, char** objects, Allocator* allocator)
{
    size_t cwd_len = cwd ? strlen(cwd) + 1 : 0;
    char* p = strtok(sources_sep_with_semicolon, "\";");
    char const* out_dir = get_var("out_dir");
    for (char* token = p; token != NULL;)
    {
        char* path = string_from_print(allocator, "%s/obj/%s" OBJ_EXT, out_dir, token + cwd_len);
        array_push(allocator, objects, path);
        token = strtok(NULL, "\";");
    }
    return objects;
}

static void print_help(bool detailed)
{
    printf("Usage: cup [options] [targets]\n");
    printf("\nOptions:\n");
    printf("  -h                            Print short help\n");
    printf("  -hh                           Print detailed help\n");
    printf("  -out_dir <dir>                Set output directory\n");
    printf("  -t <toolchain>                Set toolchain (llvm, msvc, gcc, zig)\n");
    printf("  -linker <linker>              Set LLVM -fuse-ld linker (lld(==default), link, ld, default)\n");
    printf("  -O<level>                     Set optimization level (0, 3, s)\n");
    printf("  -clean                        Clean build\n");
    printf("  -dry                          Dry run (commands skipped but treated as success)\n");
    printf("  -test                         Run tests\n");
    printf("  -r, --bootstrap               Bootstrap (ignore build.c, build cup only)\n");
    printf("  -root <dir>                   Set root directory\n");
    if (detailed)
    {
        printf("\nDetailed Options:\n");
        printf("  -h\n");
        printf("        Print a short summary of available options.\n");
        printf("  -hh\n");
        printf("        Print this detailed help with option descriptions.\n");
        printf("  -out_dir <dir>\n");
        printf("        Specify the output directory for build artifacts.\n");
        printf("        Default: build/\n");
        printf("  -t <toolchain>\n");
        printf("        Select the toolchain to use for compilation.\n");
        printf("        Supported: llvm, msvc, gcc, zig\n");
        printf("  -linker <linker>\n");
        printf("        Select the linker to use with the LLVM toolchain via -fuse-ld=<linker>.\n");
        printf("        Supported: default, lld\n");
        printf("  -O<level>\n");
        printf("        0  : Debug (no optimization)\n");
        printf("        3  : Release (optimize for speed)\n");
        printf("        s  : Release (optimize for size)\n");
        printf("  -clean\n");
        printf("        Remove all build artifacts.\n");
        printf("  -dry\n");
        printf("        Commands are not executed but treated as successful.\n");
        printf("  -test\n");
        printf("        Build and run all test executables.\n");
        printf("  -r, --bootstrap\n");
        printf("        Ignore all custom build.c files and build cup itself only.\n");
        printf("  -root <dir>\n");
        printf("        Set the project root directory. All relative paths are resolved\n");
        printf("        relative to this directory.\n");
        printf("        Default: current working directory\n");
    }
}

static void parse_cmdline(void)
{
    void c_toolchain_set_llvm_linker_type_explicit(LinkerType type);

    char const* cli = os_get_cmdline();
    char const* p = cli;
    char* arg = NULL;
    Allocator* temp_allocator = allocator_temp();
    // skip first arg
    p = utilities_split_cmd(temp_allocator, p, &arg);
    Allocator* allocator = allocator_c();
    while (true)
    {
        p = utilities_split_cmd(temp_allocator, p, &arg);
        if (array_size(arg) == 0)
        {
            break;
        }
        else
        {
            // To support multi-file selection compilation in Visual Studio
            if (strcmp(arg, "-compile") == 0)
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                char const* cwd = get_var("workspace");
                target_names = objects_from_sources_string(arg, cwd, target_names, allocator);
                continue;
            }
            else if (string_equal(arg, "-root"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(true);
                    exit(EXIT_FAILURE);
                }
                set_root_dir(arg);
                continue;
            }
            else if (string_equal(arg, "-print_exe_entries"))
            {
                b_print_exe_entries = true;
                continue;
            }
            else if (string_equal(arg, "-clean"))
            {
                b_clean = true;
                continue;
            }
            else if (string_equal(arg, "-dry"))
            {
                b_dry_run = true;
                continue;
            }
            else if (string_equal(arg, "-t"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                if (string_equal(arg, "llvm")) set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
                else if (string_equal(arg, "msvc")) set_default_toolchain(TOOLCHAIN_TYPE_MSVC);
                else if (string_equal(arg, "gcc")) set_default_toolchain(TOOLCHAIN_TYPE_GCC);
                else if (string_equal(arg, "zig")) set_default_toolchain(TOOLCHAIN_TYPE_ZIG);
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-linker"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                if (string_equal(arg, "link")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LINK);
                else if (string_equal(arg, "ld")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LD);
                else if (string_equal(arg, "lld")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LLD);
                else if (string_equal(arg, "default")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LLD);
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-test"))
            {
                b_run_tests = true;
                set_test_enabled(true);
                continue;
            }
            else if (string_equal(arg, "-r") || string_equal(arg, "--bootstrap"))
            {
                b_bootstrap = true;
                continue;
            }
            else if (arg[0] == '-' && arg[1] == 'O' && arg[2] != '\0')
            {
                char const* level = arg + 2;
                if (string_equal(level, "0"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_DEBUG);
                }
                else if (string_equal(level, "3"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_RELEASE_FAST);
                }
                else if (string_equal(level, "s"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_RELEASE_SMALL);
                }
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-out_dir"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                set_var("out_dir", arg);
                continue;
            }
            else if (string_equal(arg, "-h") || string_equal(arg, "-hh"))
            {
                print_help(string_equal(arg, "-hh"));
                exit(EXIT_SUCCESS);
            }
            else if (arg[0] == '-')
            {
                continue;
            }
            char* target = string_from_c_str(allocator, arg);
            array_push(allocator, target_names, target);
        }
    }
}

static void report(Node* cmd)
{
    int exit_code = cmd->exit_code;
    if (exit_code != EXIT_SUCCESS)
    {
        char const* desc = cmd_get_description(cmd);
        fprintf(stderr, "command failed: %s:%d:\n", cmd->file, cmd->line);
        if (array_size(desc))
        {
            fprintf(stderr, "%s\n", desc);
        }
    }
    else if (array_size(cmd->std_error) || array_size(cmd->std_output))
    {
        char const* cmdline = cmd_get_cmdline(cmd);
        if (cmdline)
        {
            fprintf(stderr, "%s\n", cmdline);
        }
        fprintf(stderr, "command output: %s:%d:\n", cmd->file, cmd->line);
    }
    else if (array_size(cmd->std_error) || array_size(cmd->std_output))
    {
        fprintf(stderr, "command output: %s:%d:\n", cmd->file, cmd->line);
        char const* cmdline = cmd_get_cmdline(cmd);
        if (cmdline)
        {
            fprintf(stderr, "%s\n", cmdline);
        }
    }
    if (array_size(cmd->std_error))
    {
        fputs(cmd->std_error, stderr);
        putc('\n', stderr);
    }
    if (array_size(cmd->std_output))
    {
        fputs(cmd->std_output, stdout);
        putc('\n', stdout);
    }
}

void restart(void)
{
    destroy();
    os_reset_env();
    // exit(0);
    char const* cmdline = os_get_cmdline();
    Process* new_process = os_start_process(cmdline);
    int exit_code = os_wait_process(new_process);
    os_forget_process(new_process);
    exit(exit_code);
}

char const* get_src_file_dir(char const* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_temp();
    char const* parent = path_parent_path(path, temp_allocator);
    char const* dir = path_lexically_normal(parent, allocator);
    return dir;
}

void determine_toolchain(void)
{
    ToolchainType c_toolchain_select_toolchain_automatically();
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_default_toolchain(c_toolchain_select_toolchain_automatically());
    }
    if (self_build_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_self_build_toolchain(default_toolchain);
    }
}

void sort_entries(void)
{
    Entry* entries = entry_get_all();
    if (entries)
    {
        qsort(entries, array_size(entries), sizeof(Entry), entry_compare);
    }
}

void invoke_entries()
{
    Entry* entries = entry_get_all();
    for (size_t i = 0; i != array_size(entries); i++)
    {
        allocator_reset_temp();
        char const* dir = get_src_file_dir(entries[i].file, allocator_temp());
        set_var("dir", dir);
        entries[i].fn();
        set_var("dir", NULL);
    }
}

static Node* wait_node(Executor* executor)
{
    Task* task = executor_wait(executor);
    if (task)
    {
        Node* node = executor_get_task_context(task);
        node->exit_code = executor_get_task_exit_code(task);
        executor_destroy_task(executor, task);
        return node;
    }
    return NULL;
}

extern char const* desc_color_error;
extern char const* desc_color_reset;

static int build(Graph* graph)
{
    int exit_code = EXIT_SUCCESS;
    size_t fail_count = 0;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    size_t num_jobs = max_jobs;
    if (num_jobs == 0)
    {
        num_jobs = os_get_cpu_count();
    }
    Executor* executor = executor_create(allocator, num_jobs);
    while (true)
    {
        allocator_reset_temp();
        Node* node = graph_pop(graph);
        if (node)
        {
            node->visit(node, graph, executor);
            if (!executor_is_full(executor)) continue;
        }
        else if (executor_is_empty(executor))
        {
            break;
        }
        node = wait_node(executor);
        if (node->exit_code == EXIT_SUCCESS)
        {
            node->after_execute(node);
            node->processed(node, graph);
        }
        else
        {
            exit_code = node->exit_code;
            fail_count++;
        }
        report(node);
        if (max_build_errors && fail_count >= max_build_errors)
        {
            break;
        }
    }

    Node** unreachable_nodes = graph_get_unreachable_nodes(graph, allocator);
    if (array_size(unreachable_nodes) && exit_code == EXIT_SUCCESS)
    {
        for (size_t i = 0; i != array_size(unreachable_nodes); i++)
        {
            Node* n = unreachable_nodes[i];
            fprintf(stderr, "%serror%s: unreachable node: %s\n", desc_color_error, desc_color_reset, n->path);
        }
    }
    executor_destroy(executor);
    allocator_destroy(allocator);
    return exit_code;
}

static int determine_build_targets(void)
{
    array_resize(allocator_c(), targets, 0);
    for (size_t i = 0; i != array_size(target_names); i++)
    {
        char const* name = target_names[i];
        Node* target = find_node(name);
        if (target)
        {
            array_push(allocator_c(), targets, target);
        }
        else
        {
            warn("Unknown target: %s", name);
            return EXIT_FAILURE;
        }
    }
    if (array_size(targets) == 0)
    {
        Node** nodes = get_all_nodes();
        for (size_t i = 0; i != array_size(nodes); i++)
        {
            Node* node = nodes[i];
            if (!node->b_default_excluded)
            {
                array_push(allocator_c(), targets, node);
            }
        }
    }
    return EXIT_SUCCESS;
}

static Node* create_run_test_target(Node* exe)
{
    Node* run_test_cmd = CMD_FROM_EXE(exe, fmt("run test: {:n}", exe));
    run_test_cmd->b_dirty = true;
    array_push(node_allocator, targets, run_test_cmd);
    node_ensure_prepared(run_test_cmd);
    return run_test_cmd;
}

static int determine_test_targets(void)
{
    uint32_t test_type = node_make_file_type(FILE_TYPE_EXE, C_FILE_TEST);
    array_resize(node_allocator, targets, 0);
    for (size_t i = 0; i != array_size(target_names); i++)
    {
        char const* name = target_names[i];
        Node* node = find_node(name);
        if (node)
        {
            if (node->type != test_type)
            {
                warn("Unknown test target: skip %s", name);
                continue;
            }
            create_run_test_target(node);
        }
        else
        {
            warn("Unknown target: %s", name);
            return EXIT_FAILURE;
        }
    }
    if (array_size(targets) == 0)
    {
        for (size_t i = 0; i != array_size(nodes); i++)
        {
            Node* node = nodes[i];
            if (node->type != test_type)
            {
                continue;
            }
            create_run_test_target(node);
        }
    }
    return EXIT_SUCCESS;
}

static void remove_generated_files(Node* target, Set* set, Node* self)
{
    bool b_existed;
    hash_insert_check(set, (uintptr_t)target, &b_existed);
    if (b_existed)
    {
        return;
    }
    Node* dll = NULL;
    if (b_dll_mode)
    {
        dll = DLL("{out_dir}/{self_name}");
    }
    if (target->node_type == NODE_TYPE_FILE && target != self)
    {
        if (target->build_cmd)
        {
            if (os_file_exists(target->path))
            {
                printf("Removing file: %s\n", target->path);
                if (!b_dry_run)
                {
                    if (target == dll)
                    {
                        void c_toolchain_rename_to_old(char const* path);
                        c_toolchain_rename_to_old(target->path);
                    }
                    else
                    {
                        os_remove_file(target->path);
                    }
                }
            }
        }
    }
    for (size_t j = 0; j != array_size(target->dependencies); j++)
    {
        remove_generated_files(target->dependencies[j], set, self);
    }
}

static void clean(void)
{
    Cache* cache = cache_load_readonly(allocator_temp(), fmt("{out_dir}/.cup_cache"));
    if (!cache)
    {
        return;
    }
    char const* dll_path = NULL;
    if (b_dll_mode)
    {
        dll_path = fmt("{out_dir}/{self_name}" DLL_EXT);
    }
    Node* self = get_or_add_file_with_type(fmt("{self}"), FILE_TYPE_EXE);
    StringPtrHash* h = cache->hash_path_to_output_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        char const* path = cache_get_string(cache, record->id);
        if (string_equal(self->path, path) || string_equal(path, "build.c"))
        {
            continue;
        }
        if (os_file_exists(path))
        {
            printf("Removing file: %s\n", path);
            if (!b_dry_run)
            {
                if (dll_path && string_equal(dll_path, path))
                {
                    void c_toolchain_rename_to_old(char const* path);
                    c_toolchain_rename_to_old(path);
                }
                else
                {
                    os_remove_file(path);
                }
            }
        }
    }
}

static Node* get_self(void)
{
    if (b_dll_mode)
    {
        return DLL("{out_dir}/{self_name}");
    }
    else
    {
        return EXE("{self_name}");
    }
}

void get_scan_test_cmds(Allocator* allocator, Node*** out_cmds);

static int scan_tests(void)
{
    int exit_code = EXIT_SUCCESS;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node** targets = NULL;
    if (b_test_enabled)
    {
        get_scan_test_cmds(allocator, &targets);
    }
    if (array_size(targets))
    {
        Graph* graph = graph_create(allocator, targets, array_size(targets));
        exit_code = build(graph);
    }
    allocator_destroy(allocator);
    return exit_code;
}

static void add_test_executables(void)
{
    Node* add_test_exe_for_obj(Node * obj, char const** entries);

    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_CMD ||
            node->cmd_type != CMD_TYPE_EXECUTABLE ||
            node->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        CCompileCmd* cc = (CCompileCmd*)node;
        if (array_size(cc->src->test_entries))
        {
            add_test_exe_for_obj(cc->out_obj, cc->src->test_entries);
        }
    }
}

static int scan_deps(void)
{
    if (!b_scan_deps_enabled)
    {
        return 0;
    }
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE);
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->type != type)
        {
            continue;
        }
        CCompileCmd* cmd = (CCompileCmd*)node;
        if (cmd->b_cpp &&
            cmd->cpp_std >= CPP_LANGUAGE_STANDARD_20 &&
            cmd->export_name == NULL &&
            array_size(cmd->import_names) == 0)
        {
            cmd->scan_deps_cmd = (ScanDepsCmd*)scan_deps_cmd_create(cmd);
            cmd->scan_deps_cmd->b_default_excluded = true;
        }
    }
    int exit_code = EXIT_SUCCESS;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    extern Node** scan_deps_cmds;
    if (array_size(scan_deps_cmds))
    {
        Graph* graph = graph_create(allocator, scan_deps_cmds, array_size(scan_deps_cmds));
        exit_code = build(graph);
    }
    allocator_destroy(allocator);
    for (size_t i = 0; i != array_size(scan_deps_cmds); i++)
    {
        ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmds[i];
        CCompileCmd* cc = scan->compile_cmd;
        if (scan->export_name)
        {
            Node* bmi = module_from_src(cc->src);
            c_compile_cmd_set_export((Node*)cc, scan->export_name, bmi);
        }
        if (cc->export_name)
        {
            Node* bmi = module_from_src(cc->src);
            c_compile_cmd_set_export((Node*)cc, cc->export_name, bmi);
        }
    }
    for (size_t i = 0; i != array_size(scan_deps_cmds); i++)
    {
        ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmds[i];
        CCompileCmd* cc = scan->compile_cmd;
        for (size_t j = 0; j != array_size(scan->imports); j++)
        {
            char const* import = scan->imports[j];
            Node* bmi = hash_get(cc->import_map, import);
            c_compile_cmd_add_import((Node*)cc, import, bmi);
        }
    }
    return exit_code;
}

static void prepare(void)
{
    for (size_t start = 0, num_nodes = array_size(nodes); start != num_nodes;)
    {
        for (size_t i = start; i != num_nodes; i++)
        {
            node_ensure_prepared(nodes[i]);
        }
        start = num_nodes;
        num_nodes = array_size(nodes);
    };
}

typedef struct VSCodeLaunchEntry
{
    char const* name;
    char const* debugger_type;
    char const* program;
    char const* group;
    char const** args;
    size_t num_args;
} VSCodeLaunchEntry;

static char* print_vscode_launch_entry(char* str, VSCodeLaunchEntry const* entry, Allocator* allocator)
{
    string_printf(allocator, str, "        {\n");
    string_printf(allocator, str, "            \"name\": \"%s\",\n", entry->name);
    string_printf(allocator, str, "            \"type\": \"%s\",\n", entry->debugger_type);
    string_printf(allocator, str, "            \"request\": \"%s\",\n", "launch");
    string_printf(allocator, str, "            \"program\": \"${workspaceFolder}/%s\",\n", entry->program);
    if (!string_equal(entry->debugger_type, "cppdbg"))
    {
        string_printf(allocator, str, "            \"console\": \"integratedTerminal\",\n");
    }
    string_printf(allocator, str, "            \"cwd\": \"${workspaceFolder}\"");
    if (entry->group)
    {
        string_printf(allocator, str, ",\n");
        string_printf(allocator, str, "            \"presentation\": {\n");
        string_printf(allocator, str, "                \"group\": \"%s\"\n", entry->group);
        string_printf(allocator, str, "            }");
    }
    if (entry->args)
    {
        bool b_first = true;
        string_printf(allocator, str, ",\n");
        string_printf(allocator, str, "            \"args\": [");
        for (size_t i = 0; i != entry->num_args; i++)
        {
            char const* arg = entry->args[i];
            if (b_first)
            {
                b_first = false;
            }
            else
            {
                string_printf(allocator, str, ",");
            }
            string_printf(allocator, str, "\"%s\"", arg);
        }
        string_printf(allocator, str, "]");
    }
    string_printf(allocator, str, "\n");
    string_printf(allocator, str, "        }");
    return str;
}

static char const* get_toolchain_vscode_debugger_type(ToolchainType toolchain)
{
    if (vscode_debugger_type == NULL)
    {
        if (CURRENT_PLATFORM != PLATFORM_WINDOWS || toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_TCC)
        {
            return "cppdbg";
        }
        return "cppvsdbg";
    }
    return vscode_debugger_type;
}

static char const* get_exe_vscode_debugger_type(Node* exe)
{
    if (exe->build_cmd && exe->build_cmd->cmd_ext_type == C_CMD_LINK)
    {
        LinkCmd* link = (LinkCmd*)exe->build_cmd;
        return get_toolchain_vscode_debugger_type(link->toolchain);
    }
    return get_toolchain_vscode_debugger_type(default_toolchain);
}

static char* print_vscode_launch_configurations(char* str, Allocator* allocator)
{
    string_printf(allocator, str, "[\n");
    bool b_first = true;
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->file_type != FILE_TYPE_EXE)
        {
            continue;
        }
        VSCodeLaunchEntry entry = {.program = node->path, .debugger_type = get_exe_vscode_debugger_type(node)};
        bool b_test = is_test_exe(node);
        if (b_test)
        {
            entry.group = node->path;
            char const** entries = get_test_entries(node);
            for (size_t j = 0; j != array_size(entries); j++)
            {
                entry.name = string_from_print(allocator_temp(), "%s %s", node->path, entries[j]);
                entry.args = &entries[j];
                entry.num_args = 1;
                if (b_first)
                {
                    b_first = false;
                }
                else
                {
                    string_printf(allocator, str, ",\n");
                }
                str = print_vscode_launch_entry(str, &entry, allocator);
            }
        }
        entry.name = node->path;
        entry.args = node->debugger_run_arguments;
        entry.num_args = array_size(node->debugger_run_arguments);
        if (b_first)
        {
            b_first = false;
        }
        else
        {
            string_printf(allocator, str, ",\n");
        }
        str = print_vscode_launch_entry(str, &entry, allocator);
    }
    if (!b_first)
    {
        string_printf(allocator, str, "\n");
    }
    string_printf(allocator, str, "    ]\n");
    return str;
}

static char* gen_vscode_launch_json_string(Allocator* allocator)
{
    char* result = NULL;
    string_printf(allocator, result, "{\n");
    string_printf(allocator, result, "    \"version\": \"0.2.0\",\n");
    string_printf(allocator, result, "    \"configurations\": ");
    result = print_vscode_launch_configurations(result, allocator);
    string_printf(allocator, result, "}");
    return result;
}

static int gen_vscode_launch_json_callback(Node* cmd)
{
    char* launch_json = cmd->extra_data;
    if (!launch_json)
    {
        launch_json = gen_vscode_launch_json_string(allocator_c());
    }
    char const* path = cmd->outputs[0]->path;
    os_write_all(path, launch_json, array_size(launch_json));
    array_free(allocator_c(), launch_json);
    return EXIT_SUCCESS;
}

static bool gen_vscode_launch_json_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    Allocator* temp_allocator = allocator_create_chained();
    char* new_launch_json = gen_vscode_launch_json_string(allocator_c());
    char const* path = cmd->outputs[0]->path;
    char* old_content = os_read_all(temp_allocator, path);
    bool b_dirty = false;
    if (old_content == NULL || !string_equal(old_content, new_launch_json))
    {
        cmd->extra_data = new_launch_json;
        b_dirty = true;
    }
    else
    {
        array_free(allocator_c(), new_launch_json);
    }
    allocator_destroy(temp_allocator);
    return b_dirty;
}

void set_generate_vscode_files_enabled(bool enabled)
{
    b_generate_vscode_files = enabled;
}

void set_vscode_debugger_type(char const* type)
{
    vscode_debugger_type = type;
}

void set_after_prepare_callback(FnAfterPrepare* fn)
{
    fn_after_prepare = fn;
}

ENTRY(gen_vscode_launch_json)
{
    Node* output = get_or_add_file(".vscode/launch.json");
    Node* cmd = CALLBACK_CMD(gen_vscode_launch_json_callback, output->path);
    node_set_alias(output, "vsc_launch");
    node_set_check_dirty_fn(cmd, gen_vscode_launch_json_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));

    output->b_default_excluded = !b_generate_vscode_files;
    cmd->b_default_excluded = !b_generate_vscode_files;
}

static char* gen_vscode_tasks_json_string(Allocator* allocator)
{
    char* result = NULL;
    string_printf(allocator, result, "{\n");
    string_printf(allocator, result, "    \"version\": \"2.0.0\",\n");
    string_printf(allocator, result, "    \"tasks\": [");
    char const* self_name = get_var("self_name");
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->build_cmd == NULL)
        {
            continue;
        }
        string_printf(allocator, result, "\n");
        string_printf(allocator, result, "        {\n");
        string_printf(allocator, result, "            \"label\": \"%s\",\n", node->path);
        string_printf(allocator, result, "            \"type\": \"process\",\n");
        string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
        string_printf(allocator, result, "            \"args\": [\"%s\"],\n", node->path);
        string_printf(allocator, result, "            \"group\": {\"kind\": \"build\"}\n");
        string_printf(allocator, result, "        },");
    }
    // All
    string_printf(allocator, result, "\n");
    string_printf(allocator, result, "        {\n");
    string_printf(allocator, result, "            \"label\": \"%s\",\n", "All");
    string_printf(allocator, result, "            \"type\": \"process\",\n");
    string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
    string_printf(allocator, result, "            \"group\": {\"kind\": \"build\", \"isDefault\": true}\n");
    string_printf(allocator, result, "        },");
    // Clean
    string_printf(allocator, result, "\n");
    string_printf(allocator, result, "        {\n");
    string_printf(allocator, result, "            \"label\": \"%s\",\n", "Clean");
    string_printf(allocator, result, "            \"type\": \"process\",\n");
    string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
    string_printf(allocator, result, "            \"args\": [\"clean\"],\n");
    string_printf(allocator, result, "            \"group\": {\"kind\": \"build\"}\n");
    string_printf(allocator, result, "        }");
    string_printf(allocator, result, "\n");

    string_printf(allocator, result, "    ]\n");
    string_printf(allocator, result, "}");
    return result;
}

static bool gen_vscode_tasks_json_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    char* new_content = gen_vscode_tasks_json_string(allocator_c());
    char const* path = cmd->outputs[0]->path;
    Allocator* allocator = allocator_create_chained();
    char const* old = os_read_all(allocator, path);
    bool b_dirty = false;
    if (old == NULL || !string_equal(new_content, old))
    {
        b_dirty = true;
        cmd->extra_data = new_content;
    }
    else
    {
        array_free(allocator_c(), new_content);
    }
    allocator_destroy(allocator);
    return b_dirty;
}

static int gen_vscode_tasks_json_callback(Node* cmd)
{
    char* content = cmd->extra_data;
    if (!content)
    {
        content = gen_vscode_tasks_json_string(allocator_c());
    }
    char const* path = cmd->outputs[0]->path;
    os_write_all(path, content, array_size(content));
    array_free(allocator_c(), content);
    return EXIT_SUCCESS;
}

ENTRY(gen_vscode_tasks_json)
{
    Node* output = get_or_add_file(".vscode/tasks.json");
    output->b_default_excluded = !b_generate_vscode_files;
    Node* cmd = CALLBACK_CMD(gen_vscode_tasks_json_callback, NULL);
    cmd->b_default_excluded = !b_generate_vscode_files;
    node_set_alias(output, "vsc_tasks");
    node_set_check_dirty_fn(cmd, gen_vscode_tasks_json_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));
}

static int print_exe_entries(void)
{
    Allocator* temp_allocator = allocator_temp();
    char* configurations = string_from_c_str(temp_allocator, "    ");
    configurations = print_vscode_launch_configurations(configurations, temp_allocator);
    puts(configurations);
    return 0;
}

void collect_build_scripts(char const* directory, Allocator* allocator)
{
    extern StringSet* build_scripts;
    Allocator* temp_allocator = allocator_temp();
    Directory* d = directory_open(directory, temp_allocator);
    if (d)
    {
        while (true)
        {
            DirectoryEntry* entry = directory_read(d);
            if (!entry)
            {
                directory_close(d);
                break;
            }
            if (string_equal(entry->name, ".") || string_equal(entry->name, ".."))
            {
                continue;
            }
            if (entry->is_directory)
            {
                char const* sub_dir = string_from_print(temp_allocator, "%s/%s", directory, entry->name);
                collect_build_scripts(sub_dir, allocator);
            }
            else
            {
                if (string_equal(entry->name, "build.c"))
                {
                    bool b_existed;
                    char* path = string_from_print(allocator, "%s/%s", directory, entry->name);
                    hash_insert_check(build_scripts, path, &b_existed);
                    if (b_existed)
                    {
                        array_free(allocator, path);
                    }
                }
            }
        }
    }
}

void set_cup_h_dir(char const* dir)
{
    cup_h_dir = dir;
}

void set_node_default_excluded(bool b_default_excluded)
{
    b_node_default_excluded = b_default_excluded;
}

void set_content_hash_enabled(bool b_enabled)
{
    b_content_hash = b_enabled;
}

static Node* add_copy_cmd_windows(Node* input, Node* output, char const* file, int line)
{
    char* src = string_new(allocator_temp(), array_size(input->path), input->path);
    char* dst = string_new(allocator_temp(), array_size(output->path), output->path);
    path_slash_to_backslash(src);
    path_slash_to_backslash(dst);
    if (file_path_has_space(input))
    {
        src = string_from_print(allocator_temp(), "\"%s\"", src);
    }
    if (file_path_has_space(output))
    {
        dst = string_from_print(allocator_temp(), "\"%s\"", dst);
    }
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    char const* name = fmt("copy: output: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    cmd_add_option(node, OPTION_EXE, "cmd /c copy");
    cmd_add_option(node, OPTION_INPUT, src);
    cmd_add_option(node, OPTION_OUTPUT, dst);
    cmd_add_option(node, OPTION_FLAG, "/Y");
    cmd_add_option(node, OPTION_FLAG, "> nul");
    cmd_add_input(node, input);
    cmd_add_output(node, output);
    return node;
}

static Node* add_copy_cmd_linux(Node* input, Node* output, char const* file, int line)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    char const* name = fmt("copy: output: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    cmd_add_option(node, OPTION_EXE, "cp");
    cmd_add_input_file_option(node, input);
    cmd_add_output_file_option(node, output);
    return node;
}

Node* add_copy_cmd(Node* input, Node* output, char const* file, int line)
{
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return add_copy_cmd_windows(input, output, file, line);
    }
    return add_copy_cmd_linux(input, output, file, line);
}

ArchitectureType get_self_build_arch(void)
{
    return CURRENT_ARCHITECTURE;
}

int build_self(void)
{
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node* self = get_self();
    Graph* graph = graph_create(allocator, &self, 1);
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static int build_targets(void)
{
    determine_build_targets();
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static void cmd_visit_dry_run(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    char const* desc = cmd_get_description(node);
    if (array_size(desc))
    {
        puts(desc);
    }
    node->processed(node, graph);
}

static int dry_run(void)
{
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node** targets = get_all_nodes();
    for (size_t i = 0; i != array_size(targets); i++)
    {
        Node* node = targets[i];
        if (node->node_type != NODE_TYPE_CMD)
        {
            continue;
        }
        Cmd* cmd = (Cmd*)node;
        cmd->visit = cmd_visit_dry_run;
    }
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static int run_tests()
{
    build_self();
    determine_test_targets();
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    max_build_errors = array_size(targets);
    int exit_code = build(graph);
    max_build_errors = 1;
    size_t total = 0;
    size_t passed = 0;
    for (size_t i = 0; i != array_size(targets); i++)
    {
        total++;
        if (targets[i]->exit_code == EXIT_SUCCESS)
        {
            passed++;
        }
    }
    if (total)
    {
        fprintf(stdout, "\nresults: %zu total, %zu passed, %zu failed\n", total, passed, total - passed);
    }
    allocator_destroy(allocator);
    return exit_code;
}

void init_mode(void);
void init_toolchain(void);
bool c_toolchain_is_linker_type_explicit(void);
void c_toolchain_restore_llvm_linker_type(LinkerType type);

static void read_last_status(void)
{
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED ||
        default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED ||
        !c_toolchain_is_linker_type_explicit())
    {
        char const* status_path = fmt("{out_dir}/.last_status");
        char* content = os_read_all(allocator_temp(), status_path);
        if (content)
        {
            char* line = strtok(content, "\r\n");
            if (line && default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
            {
                if (string_equal(line, "llvm")) set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
                else if (string_equal(line, "msvc")) set_default_toolchain(TOOLCHAIN_TYPE_MSVC);
                else if (string_equal(line, "gcc")) set_default_toolchain(TOOLCHAIN_TYPE_GCC);
                else if (string_equal(line, "zig")) set_default_toolchain(TOOLCHAIN_TYPE_ZIG);
                else if (string_equal(line, "tcc")) set_default_toolchain(TOOLCHAIN_TYPE_TCC);
            }
            line = strtok(NULL, "\r\n");
            if (line && default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED)
            {
                if (string_equal(line, "debug")) set_default_optimization(OPTIMIZATION_TYPE_DEBUG);
                else if (string_equal(line, "release_fast")) set_default_optimization(OPTIMIZATION_TYPE_RELEASE_FAST);
                else if (string_equal(line, "release_small")) set_default_optimization(OPTIMIZATION_TYPE_RELEASE_SMALL);
            }
            line = strtok(NULL, "\r\n");
            if (line && !c_toolchain_is_linker_type_explicit() && default_toolchain == TOOLCHAIN_TYPE_LLVM)
            {
                if (string_equal(line, "link")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LINK);
                else if (string_equal(line, "ld")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LD);
                else if (string_equal(line, "lld")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LLD);
                else if (string_equal(line, "default")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LLD);
            }
        }
    }
}

void save_last_status(void)
{
    char const* out_dir = get_var("out_dir");
    os_create_directory_tree(out_dir);
    char const* status_path = fmt("{out_dir}/.last_status");
    char const* tc_str = NULL;
    switch (default_toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC: tc_str = "msvc"; break;
    case TOOLCHAIN_TYPE_LLVM: tc_str = "llvm"; break;
    case TOOLCHAIN_TYPE_ZIG: tc_str = "zig"; break;
    case TOOLCHAIN_TYPE_GCC: tc_str = "gcc"; break;
    case TOOLCHAIN_TYPE_TCC: tc_str = "tcc"; break;
    default: tc_str = "unspecified"; break;
    }
    char const* opt_str = NULL;
    switch (default_optimization_type)
    {
    case OPTIMIZATION_TYPE_DEBUG: opt_str = "debug"; break;
    case OPTIMIZATION_TYPE_RELEASE_FAST: opt_str = "release_fast"; break;
    case OPTIMIZATION_TYPE_RELEASE_SMALL: opt_str = "release_small"; break;
    default: opt_str = "release"; break;
    }
    char const* linker_str = "";
    if (default_toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        switch (get_llvm_linker_type())
        {
        case LINKER_LLVM_LD: linker_str = "ld"; break;
        case LINKER_LLVM_LLD: linker_str = "lld"; break;
        case LINKER_LLVM_LINK: linker_str = "link"; break;
        default: linker_str = "default"; break;
        }
    }
    char* content = string_from_print(allocator_temp(), "%s\n%s\n%s\n", tc_str, opt_str, linker_str);
    os_write_all(status_path, content, array_size(content));
}

void set_root_dir(char const* dir)
{
    extern void var_on_cwd_changed(void);

    if (!os_file_exists(dir))
    {
        error("root directory not found");
        exit(EXIT_FAILURE);
    }
    os_set_cwd(dir);
    var_on_cwd_changed();
}

CONSTRUCTOR(init)
static void init(void)
{
    os_set_console_utf8();
    init_cwd = os_get_cwd(allocator_c());
    init_var();
    parse_cmdline();
    read_last_status();
    init_node();
    init_toolchain();
    if (atexit(destroy) != 0)
    {
        printf("atexit failed!\n");
        exit(EXIT_FAILURE);
    }
    init_mode();
}

int execute(void)
{
    determine_toolchain();
    sort_entries();
    char const* lock_path = fmt("{out_dir}/.cup_lock");
    process_lock_ctx = os_lock_file(lock_path, allocator_c(), false);
    if (b_clean)
    {
        clean();
        return 0;
    }
    init_cache();
    if (b_clean)
    {
        clean();
        return 0;
    }
    invoke_entries();
    int exit_code;
    if (b_test_enabled)
    {
        exit_code = scan_tests();
        add_test_executables();
        if (exit_code == EXIT_FAILURE)
        {
            return exit_code;
        }
    }
    if (b_scan_deps_enabled)
    {
        exit_code = scan_deps();
        if (exit_code != EXIT_SUCCESS)
        {
            return exit_code;
        }
    }
    prepare();
    if (fn_after_prepare)
    {
        fn_after_prepare();
    }
    if (b_print_exe_entries)
    {
        return print_exe_entries();
    }
    if (b_dry_run)
    {
        return dry_run();
    }
    if (b_run_tests)
    {
        exit_code = run_tests();
        return exit_code;
    }
    exit_code = build_self();
    if (exit_code != EXIT_SUCCESS)
    {
        return exit_code;
    }
    exit_code = build_targets();
    return exit_code;
}
