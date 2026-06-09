#include "cup/node.h"
#include "core/allocator.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "core/utilities.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/cache.h"
#include "cup/depfile.h"
#include "cup/executor/executor.h"
#include "cup/fmt.h"
#include "cup/graph.h"

bool b_node_default_excluded = false;

Allocator* node_allocator;
Node** nodes;
StringPtrHash* hash_name_to_node;

extern char const* desc_color_exe;
extern char const* desc_color_input;
extern char const* desc_color_output;
extern char const* desc_color_reset;
extern char const* desc_color_error;
extern char const* desc_color_flag;
extern char const* desc_color_bright_flag;
extern bool b_content_hash;

SourceType get_source_type(char const* path)
{
    char const* ext_old = path_extension(path);
    char* ext = string_from_c_str(allocator_temp(), ext_old);
    string_tolower(ext);
    if (string_equal(ext, ".c"))
    {
        return SOURCE_TYPE_C;
    }
    if (string_equal(ext, ".cpp") || string_equal(ext, ".cc"))
    {
        return SOURCE_TYPE_CPP;
    }
    if (string_equal(ext, ".cppm") || string_equal(ext, ".ixx") ||
        string_equal(ext, ".ccm") || string_equal(ext, ".cxxm"))
    {
        return SOURCE_TYPE_CPPM;
    }
    if (string_equal(ext, ".s") || string_equal(ext, ".asm"))
    {
        return SOURCE_TYPE_ASM;
    }
    return SOURCE_TYPE_UNKNOWN;
}

void init_node(void)
{
    node_allocator = allocator_create_tiny(4096, 4096 * 4096);
    hash_name_to_node = allocator_calloc(node_allocator, 1, sizeof(StringPtrHash));
    expect(hash_name_to_node, "allocation failed");
    hash_name_to_node->allocator = node_allocator;
}

uint32_t node_make_file_type(FileType file_type, uint32_t ext_type)
{
    Node node;
    memset(&node, 0, sizeof(node));
    node.file_node_type = NODE_TYPE_FILE;
    node.file_type = file_type;
    node.file_ext_type = ext_type;
    return node.type;
}

uint32_t node_make_cmd_type(CmdType cmd_type, uint32_t ext_type)
{
    Node node;
    memset(&node, 0, sizeof(node));
    node.cmd_node_type = NODE_TYPE_CMD;
    node.cmd_type = cmd_type;
    node.cmd_ext_type = ext_type;
    return node.type;
}

uint32_t node_make_virtual_type(uint32_t ext_type)
{
    Node node;
    memset(&node, 0, sizeof(node));
    node.node_type = NODE_TYPE_VIRTUAL;
    node.virtual_ext_type = ext_type;
    return node.type;
}

void node_prepare(Node* node)
{
}

void node_visit(Node* node, Graph* graph, Executor* executor)
{
    if (!node->b_dirty)
    {
        node->b_dirty = node->check_dirty(node);
    }
}

void node_virtual_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    node->processed(node, graph);
}

bool node_virtual_check_dirty(Node* node)
{
    return true;
}

void node_processed(Node* node, Graph* graph)
{
    graph_set_node_processed(graph, node);
}

Node* node_create(uint32_t type, char const* name, size_t num_bytes)
{
    Node* node = allocator_calloc(node_allocator, 1, num_bytes);
    expect(node, "node is NULL");
    node->type = type;
    node->b_default_excluded = b_node_default_excluded;
    if (name)
    {
        node_set_name(node, name);
    }
    node->prepare = node_prepare;
    switch (node->node_type)
    {
    case NODE_TYPE_VIRTUAL:
        node->visit = node_virtual_visit;
        node->check_dirty = node_virtual_check_dirty;
        node->processed = node_processed;
        break;
    case NODE_TYPE_CMD:
        node->visit = cmd_visit;
        node->check_dirty = cmd_check_dirty;
        node->before_execute = cmd_before_execute;
        node->after_execute = cmd_after_execute;
        node->processed = cmd_processed;
        node->prepare = cmd_prepare;
        if (node->cmd_type == CMD_TYPE_EXECUTABLE)
        {
            node->write_stdout_line_fn = cmd_write_stdout_line;
            node->write_stderr_line_fn = cmd_write_stderr_line;
        }
        break;
    case NODE_TYPE_FILE:
        node->visit = file_visit;
        node->check_dirty = file_check_dirty;
        node->mtime = os_get_mtime(name);
        node->processed = file_processed;
        break;
    }
    array_push(node_allocator, nodes, node);
    return node;
}

void node_set_name(Node* node, char const* name)
{
    bool b_existed;
    uint32_t i = hash_insert_check(hash_name_to_node, name, &b_existed);
    if (b_existed)
    {
        warn("The node named \"%s\" already exists.", name);
    }
    if (array_size(node->name))
    {
        uint32_t old_i = hash_index(hash_name_to_node, node->name);
        if (old_i != HASH_INVALID_INDEX)
        {
            hash_remove(hash_name_to_node, old_i);
        }
        string_clear(node->name);
    }
    node->name = string_append_c_str(node_allocator, node->name, name);
    hash_key(hash_name_to_node, i) = node->name;
    hash_value(hash_name_to_node, i) = node;
}

void node_set_alias(Node* node, char const* alias)
{
    bool b_existed;
    uint32_t i = hash_insert_check(hash_name_to_node, alias, &b_existed);
    if (b_existed)
    {
        warn("The node named \"%s\" already exists.", alias);
    }
    alias = string_from_c_str(node_allocator, alias);
    hash_key(hash_name_to_node, i) = alias;
    hash_value(hash_name_to_node, i) = node;
}

void node_add_debugger_argument(Node* node, char const* arg)
{
    char* new_arg = string_from_c_str(node_allocator, arg);
    array_push(node_allocator, node->debugger_run_arguments, new_arg);
}

void node_add_dependency(Node* node, Node* dependency)
{
    array_push(node_allocator, node->dependencies, dependency);
}

void node_ensure_prepared(Node* node)
{
    if (node->b_prepared) return;
    node->b_prepared = true;
    node->prepare(node);
}

void node_remove_dependency(Node* node, Node* dependency)
{
    array_compact(node->dependencies, array_pointer_compare, &dependency);
}

void node_set_check_dirty_fn(Node* node, FnCheckDirty* fn)
{
    node->check_dirty = fn;
}

void node_set_processed_fn(Node* node, FnProcessed* fn)
{
    node->processed = fn;
}

void node_set_extra_data(Node* node, void* extra_data)
{
    node->extra_data = extra_data;
}

void node_remove_edge(Node* tail, Node* head)
{
    array_compact(head->dependencies, array_pointer_compare, &tail);
}

static void file_check_space_and_backslash(Node* file)
{
    file->b_path_checked = true;
    for (size_t i = 0; i != array_size(file->path); i++)
    {
        int ch = file->path[i];
        if (ch == ' ')
        {
            file->b_has_space = true;
            if (file->b_has_backslash)
            {
                return;
            }
        }
        if (ch == '\\')
        {
            file->b_has_backslash = true;
            if (file->b_has_space)
            {
                return;
            }
        }
    }
}

bool file_path_has_space(Node* node)
{
    if (!node->b_path_checked)
    {
        file_check_space_and_backslash(node);
    }
    return node->b_has_space;
}

bool file_path_has_backslash(Node* node)
{
    if (!node->b_path_checked)
    {
        file_check_space_and_backslash(node);
    }
    return node->b_has_backslash;
}

uint64_t file_get_content_hash(Node* node)
{
    if (node->content_hash == 0 || node->b_dirty)
    {
        node->content_hash = utilities_compute_file_hash(node->path);
    }
    return node->content_hash;
}

char const* file_get_option_path(Node* node)
{
    char const* path;
    if (file_path_has_space(node))
    {
        path = fmt("\"{:n}\"", node);
    }
    else
    {
        path = node->path;
    }
    return path;
}

bool file_check_dirty(Node* node)
{
    if (node->mtime == 0)
    {
        return true;
    }
    if (node->build_cmd && node->build_cmd->b_dirty)
    {
        return true;
    }
    return false;
}

void file_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    node->processed(node, graph);
}

void file_processed(Node* node, Graph* graph)
{
    node_processed(node, graph);
}

Node* file_create(char const* path, size_t num_bytes)
{
    uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
    return node_create(node_type, path, num_bytes);
}

bool cmd_check_cache_dirty(Node* cmd, Cache* cache, CacheRecordCmd* r, bool* b_restat)
{
    if (r == NULL)
    {
        return true;
    }
    if (array_size(r->inputs) != array_size(cmd->inputs) || array_size(r->outputs) != array_size(cmd->outputs))
    {
        return true;
    }
    for (size_t i = 0; i != array_size(cmd->inputs); i++)
    {
        CacheFile* cf = &r->inputs[i];
        Node* input = cmd->inputs[i];
        char const* cache_file_path = cache_get_string(cache, cf->id);
        char const* input_path = input->path;
        if (!string_equal(input_path, cache_file_path))
        {
            return true;
        }
        if (input->mtime != cf->build_time)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(input);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = input->mtime;
            }
        }
    }
    for (size_t i = 0; i != array_size(cmd->outputs); i++)
    {
        Node* output = cmd->outputs[i];
        CacheFile* cf = &r->outputs[i];
        char const* output_path = output->path;
        char const* cache_file_path = cache_get_string(cache, cf->id);
        if (!string_equal(output_path, cache_file_path))
        {
            return true;
        }
        if (output->mtime != cf->build_time)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(output);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = output->mtime;
            }
        }
    }
    if (cmd->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        if (array_size(cmd->cmdline) != array_size(r->cmdline))
        {
            return true;
        }
        if (!string_equal(cmd->cmdline, r->cmdline))
        {
            return true;
        }
    }
    for (size_t i = 0; i != array_size(r->implicit_inputs); i++)
    {
        CacheFile* cf = &r->implicit_inputs[i];
        char const* path = cache_get_string(cache, cf->id);
        Node* input = find_node(path);
        if (!input)
        {
            uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
            input = node_create(node_type, path, sizeof(Node));
            input->b_default_excluded = true;
        }
        if (input->mtime == 0 || cf->build_time != input->mtime)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(input);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = input->mtime;
            }
        }
    }
    return false;
}

bool cmd_check_dirty(Node* node)
{
    for (size_t i = 0; i != array_size(node->inputs); i++)
    {
        Node* input = node->inputs[i];
        if (input->b_dirty)
        {
            return true;
        }
    }
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        if (node->outputs[i]->mtime == 0)
        {
            return true;
        }
    }
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordCmd* record = cache_find_cmd_record(cache, node->name);
        bool b_restat = false;
        if (cmd_check_cache_dirty(node, cache, record, &b_restat))
        {
            return true;
        }
        else if (b_restat)
        {
            cache_write_cmd_record(cache, record);
        }
    }
    else
    {
        return true;
    }
    return false;
}

static int thread_task_fn_wrapper(Task* task, void* ctx)
{
    Node* cmd = ctx;
    return cmd->fn(cmd);
}

static void cmd_write_output(
    Node* cmd,
    char** output_line,
    void (*write_line_fn)(Node* cmd, char const* line),
    char const* buffer,
    size_t num_bytes)
{
    for (char const* p = buffer; p != buffer + num_bytes; p++)
    {
        if (*p == '\n')
        {
            if (array_size(*output_line) && array_last(*output_line) == '\r')
            {
                array_pop(*output_line);
            }
            array_push(node_allocator, *output_line, '\0');
            array_pop(*output_line);
            write_line_fn(cmd, *output_line);
            array_resize(node_allocator, *output_line, 0);
        }
        else
        {
            array_push(node_allocator, *output_line, *p);
        }
    }
    if (num_bytes == 0 && array_size(*output_line))
    {
        array_push(node_allocator, *output_line, '\0');
        array_pop(*output_line);
        write_line_fn(cmd, *output_line);
    }
}

static void cmd_write_stderr_fn_wrapper(void* ctx, char const* buffer, size_t num_bytes)
{
    Node* cmd = ctx;
    cmd_write_output(cmd, &cmd->stderr_line, cmd->write_stderr_line_fn, buffer, num_bytes);
}

static void cmd_write_stdout_fn_wrapper(void* ctx, char const* buffer, size_t num_bytes)
{
    Node* cmd = ctx;
    cmd_write_output(cmd, &cmd->stdout_line, cmd->write_stdout_line_fn, buffer, num_bytes);
}

void cmd_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    if (node->b_dirty)
    {
        if (executor)
        {
            node->before_execute(node);
            Task* task = cmd_create_task(node, executor);
            executor_add_task(executor, task);
        }
    }
    else
    {
        node->processed(node, graph);
    }
}

void cmd_prepare(Node* node)
{
    if (node->name == NULL)
    {
        node->name = fmt_alloc(node_allocator, "run: {}", node->cmdline);
    }
    if (node->depfile && node->b_keep_depfile)
    {
        cmd_add_output(node, node->depfile);
    }
}

void cmd_processed(Node* node, Graph* graph)
{
    node_processed(node, graph);
}

void cmd_add_input(Node* node, Node* file)
{
    for (size_t i = 0; i != array_size(node->inputs); i++)
    {
        Node* input = node->inputs[i];
        if (input == file)
        {
            return;
        }
    }
    array_push(node_allocator, node->inputs, file);
    node_add_dependency(node, file);
}

void cmd_add_output(Node* node, Node* file)
{
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        if (output == file)
        {
            return;
        }
    }
    array_push(node_allocator, node->outputs, file);
    file->build_cmd = node;
    node_add_dependency(file, node);
    if (node->name == NULL)
    {
        string_printf(node_allocator, node->name, "cmd: make %s", file->path);
    }
}

void cmd_update_output_mtime(Node* node)
{
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        uint64_t new_mtime = os_get_mtime(output->path);
        if (new_mtime != output->mtime)
        {
            output->mtime = new_mtime;
            output->b_dirty = true;
        }
    }
}

void cmd_after_execute_parse_depfile(Node* node)
{
    FILE* f = os_fopen(node->depfile->path, "rb");
    if (!f)
    {
        return;
    }
    DepfileParser parser;
    depfile_parser_init(&parser, f);
    char* dep = NULL;
    DepfileItemType item_type;
    Allocator* allocator = allocator_create_tiny(4096, 4096);
    while (depfile_parser_next(&parser, allocator, &dep, &item_type))
    {
        switch (item_type)
        {
        case DEPFILE_ITEM_NORMAL_DEP:
            if (os_file_exists(dep))
            {
                cmd_add_implicit_input(node, dep);
            }
            break;
        default:;
        }
        array_resize(allocator, dep, 0);
    }
    allocator_destroy(allocator);
    fclose(f);
    if (!node->b_keep_depfile)
    {
        os_remove_file(node->depfile->path);
    }
}

void cmd_after_execute(Node* node)
{
    if (node->depfile)
    {
        cmd_after_execute_parse_depfile(node);
    }
    cmd_update_output_mtime(node);
    Cache* cache = get_cache();
    if (cache)
    {
        Allocator* temp_allocator = allocator_temp();
        CacheRecordCmd cmd_record = {0};
        cmd_record.name = node->name;
        if (node->cmd_type == CMD_TYPE_EXECUTABLE)
        {
            cmd_record.cmdline = node->cmdline;
        }
        size_t num_inputs = array_size(node->inputs);
        array_resize(temp_allocator, cmd_record.inputs, num_inputs);
        for (size_t i = 0; i != num_inputs; i++)
        {
            Node* file_node = node->inputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_in_file_record(cache, path);
            cmd_record.inputs[i].id = record->id;
            cmd_record.inputs[i].build_time = file_node->mtime;
            cmd_record.inputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        size_t num_outputs = array_size(node->outputs);
        array_resize(temp_allocator, cmd_record.outputs, num_outputs);
        for (size_t i = 0; i != num_outputs; i++)
        {
            Node* file_node = node->outputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_out_file_record(cache, path);
            cmd_record.outputs[i].id = record->id;
            cmd_record.outputs[i].build_time = file_node->mtime;
            cmd_record.outputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        size_t num_implicit_inputs = array_size(node->implicit_inputs);
        array_resize(temp_allocator, cmd_record.implicit_inputs, num_implicit_inputs);
        for (size_t i = 0; i != num_implicit_inputs; i++)
        {
            Node* file_node = node->implicit_inputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_in_file_record(cache, path);
            cmd_record.implicit_inputs[i].id = record->id;
            cmd_record.implicit_inputs[i].build_time = file_node->mtime;
            cmd_record.implicit_inputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        cache_write_cmd_record(cache, &cmd_record);
    }
}

void cmd_before_execute(Node* node)
{
    char const* desc = cmd_get_description(node);
    if (array_size(desc))
    {
        puts(desc);
    }
    array_resize(node_allocator, node->implicit_inputs, 0);
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        os_ensure_dir_existed(output->path);
    }
    if (node->depfile)
    {
        os_ensure_dir_existed(node->depfile->path);
    }
}

void cmd_add_input_file_option(Node* node, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option(node, OPTION_INPUT, path);
    cmd_add_input(node, file);
}

void cmd_add_input_file_option_no_sep(Node* node, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option_no_sep(node, OPTION_INPUT, path);
    cmd_add_input(node, file);
}

void cmd_add_output_file_option(Node* node, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option(node, OPTION_OUTPUT, path);
    cmd_add_output(node, file);
}

void cmd_add_output_file_option_no_sep(Node* node, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option_no_sep(node, OPTION_OUTPUT, path);
    cmd_add_output(node, file);
}

static char const* get_option_color(OptionType type)
{
    if (type == OPTION_EXE) return desc_color_exe;
    if (type == OPTION_INPUT) return desc_color_input;
    if (type == OPTION_OUTPUT) return desc_color_output;
    if (type == OPTION_FLAG) return desc_color_flag;
    if (type == OPTION_BRIGHT_FLAG) return desc_color_bright_flag;
    if (type == OPTION_HIDDEN) return NULL;
    fatal("unreachable");
    return NULL;
}

static void cmd_add_option_impl(Node* node, OptionType type, char const* option, bool add_sep)
{
    if (type == OPTION_HIDDEN)
    {
        if (option)
        {
            if (array_size(node->extra_options))
            {
                string_putc(node_allocator, node->extra_options, ' ');
            }
            string_printf(node_allocator, node->extra_options, "%s", option);
        }
        return;
    }
    if (add_sep && type != OPTION_EXE)
    {
        string_putc(node_allocator, node->cmdline, ' ');
        string_putc(node_allocator, node->description, ' ');
    }
    if (!option)
    {
        return;
    }
    string_concat_c_str(node_allocator, node->cmdline, option);
    char const* color = get_option_color(type);
    if (color)
    {
        string_printf(node_allocator, node->description, "%s%s%s", color, option, desc_color_reset);
    }
    else
    {
        string_concat_c_str(node_allocator, node->description, option);
    }
}

void cmd_add_option(Node* node, OptionType type, char const* option)
{
    cmd_add_option_impl(node, type, option, true);
}
void cmd_add_option_no_sep(Node* node, OptionType type, char const* option)
{
    cmd_add_option_impl(node, type, option, false);
}

void cmd_remove_input(Node* node, Node* file)
{
    node_remove_dependency(node, file);
    array_compact(node->inputs, array_pointer_compare, &file);
}

void cmd_remove_output(Node* node, Node* file)
{
    node_remove_dependency(file, node);
    array_compact(node->outputs, array_pointer_compare, &file);
}

char const* cmd_get_description(Node* node)
{
    char const* desc = node->description;
    if (!desc && node->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        return node->cmdline;
    }
    return desc;
}

char const* cmd_get_cmdline(Node* node)
{
    if (node->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        return node->cmdline;
    }
    return NULL;
}

Task* cmd_create_task(Node* cmd, Executor* executor)
{
    Task* task;
    if (cmd->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        Allocator* temp_allocator = allocator_temp();
        char* cmdline = cmd->cmdline;
        if (cmd->extra_options)
        {
            cmdline = string_from_print(temp_allocator, "%s %s", cmdline, cmd->extra_options);
        }
        task = executor_create_process_task(executor, cmdline);
        executor_set_task_write_stdout_fn(task, cmd_write_stdout_fn_wrapper);
        executor_set_task_write_stderr_fn(task, cmd_write_stderr_fn_wrapper);
        if (cmd->env_node)
        {
            executor_set_task_env_block(task, cmd->env_node->env_block);
        }
    }
    else
    {
        task = executor_create_thread_task(executor, thread_task_fn_wrapper, cmd);
    }
    executor_set_task_context(task, cmd);
    return task;
}

void cmd_set_source_location(Node* node, char const* file, int line)
{
    node->file = file;
    node->line = line;
}

void cmd_set_before_execute_fn(Node* node, FnBeforeExecute* fn)
{
    node->before_execute = fn;
}

void cmd_set_after_execute_fn(Node* node, FnAfterExecute* fn)
{
    node->after_execute = fn;
}

void cmd_set_env(Node* node, Node* env)
{
    expect(!env || env->node_type == NODE_TYPE_FILE, "env must be NULL or a FILE node");
    expect(node->node_type == NODE_TYPE_CMD && node->cmd_type == CMD_TYPE_EXECUTABLE, "node must be a CMD of type EXECUTABLE");
    if (node->env_node)
    {
        cmd_remove_input(node, node->env_node);
    }
    if (env)
    {
        cmd_add_input(node, env);
        node->env_node = env;
    }
    else
    {
        node->env_node = NULL;
        return;
    }
    if (env->file_type != FILE_TYPE_ENV)
    {
        env->file_type = FILE_TYPE_ENV;
    }
}

void cmd_write_stdout_line(Node* cmd, char const* line)
{
    size_t num_bytes = array_size(line);
    array_push_v(node_allocator, cmd->std_output, line, num_bytes);
    string_putc(node_allocator, cmd->std_output, '\n');
}

void cmd_write_stderr_line(Node* cmd, char const* line)
{
    size_t num_bytes = array_size(line);
    array_push_v(node_allocator, cmd->std_error, line, num_bytes);
    string_putc(node_allocator, cmd->std_error, '\n');
}

void cmd_set_out_depfile(Node* cmd, Node* depfile)
{
    cmd->depfile = depfile;
}

void cmd_set_write_output_line_fn(Node* node, void (*fn)(Node* cmd, char const* line))
{
    node->write_stdout_line_fn = fn;
}

void cmd_set_write_stderr_line_fn(Node* node, void (*fn)(Node* cmd, char const* line))
{
    node->write_stderr_line_fn = fn;
}

void cmd_add_implicit_input(Node* node, char const* dep)
{
    Node* file = get_or_add_file(dep);
    array_push(node_allocator, node->implicit_inputs, file);
}

void cmd_set_description(Node* node, char const* string)
{
    array_resize(node_allocator, node->description, 0);
    string_concat_c_str(node_allocator, node->description, string);
}

Node** get_all_nodes(void)
{
    return nodes;
}

Node* get_or_add_file(char const* path)
{
    Node* node = find_node(path);
    if (!node)
    {
        uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
        node = node_create(node_type, path, sizeof(Node));
    }
    return node;
}

Node* get_or_add_file_with_type(char const* path, FileType type)
{
    Node* file = get_or_add_file(path);
    file->file_type = type;
    return file;
}

Node* get_or_add_node(uint32_t type, char const* name, size_t num_bytes)
{
    Node* node = find_node(name);
    if (!node)
    {
        node = node_create(type, name, num_bytes);
    }
    return node;
}

Node* add_thread_cmd(FnThread* fn, void* ctx, char const* file, int line)
{
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_THREAD, 0);
    Node* node = node_create(node_type, NULL, sizeof(Node));
    node->fn = fn;
    node->ctx = ctx;
    cmd_set_source_location(node, file, line);
    return node;
}

Node* add_process_cmd(char const* string, char const* file, int line)
{
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    Node* n = node_create(node_type, NULL, sizeof(Node));
    if (string)
    {
        cmd_add_option(n, OPTION_EXE, string);
    }
    cmd_set_source_location(n, file, line);
    return n;
}

Node* add_process_cmd_from_exe_node(Node* exe, char const* name, char const* file, int line)
{
    Allocator* allocator = allocator_temp();
    char* cmdline;
    if (file_path_has_space(exe))
    {
        cmdline = string_from_print(allocator, "\"%s\"", exe->path);
    }
    else
    {
        cmdline = string_new(allocator, array_size(exe->path), exe->path);
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        path_slash_to_backslash(cmdline);
    }
    Node* cmd = add_process_cmd(cmdline, file, line);
    cmd_add_input(cmd, exe);
    node_set_name(cmd, name);
    return cmd;
}

Node* find_node(char const* name)
{
    expect(hash_name_to_node, "hash_name_to_node is NULL");
    return (Node*)hash_get(hash_name_to_node, name);
}

bool has_dependency(Node* node, Node* dependency)
{
    return array_find(node->dependencies, array_pointer_compare, &dependency);
}
