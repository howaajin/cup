#include "cup/entry.h"

#include "core/array.h"
#include "core/os.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/fmt.h"
#include "cup/node.h"

#include <assert.h>
#include <stdbool.h>

extern Allocator* node_allocator;

bool b_generate_compile_commands = true;
char const* compile_commands_json_path = NULL;

static char* gen_compile_commands_json_escape(char const* cstr, Allocator* allocator)
{
    char* result = string_from_c_str(allocator, "");
    for (uint64_t i = 0; cstr[i] != '\0'; i++)
    {
        if (cstr[i] == '\\')
        {
            string_printf(allocator, result, "\\\\");
        }
        else if (cstr[i] == '"')
        {
            string_printf(allocator, result, "\\\"");
        }
        else
        {
            string_printf(allocator, result, "%c", cstr[i]);
        }
    }
    return result;
}

static char* get_compile_commands_string(Node* cmd, Allocator* allocator)
{
    char* result = NULL;
    Allocator* temp_allocator = allocator_create_chained();
    char const* cwd = gen_compile_commands_json_escape(os_get_cwd(temp_allocator), temp_allocator);
    string_printf(allocator, result, "[");
    Node** nodes = cmd->dependencies;
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        assert(node->node_type == NODE_TYPE_VIRTUAL && node->virtual_ext_type == VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE);
        CompileCmdline* cmdline = (CompileCmdline*)node;
        CCompileCmd* compile_cmd = cmdline->cmd;
        char const* cmd_str = gen_compile_commands_json_escape(compile_cmd->cmdline, temp_allocator);
        static bool b_first = true;
        if (!b_first)
        {
            string_putc(allocator, result, ',');
        }
        else
        {
            b_first = false;
        }
        string_printf(allocator, result, "\n");
        string_printf(allocator, result, "    {\n");
        string_printf(allocator, result, "        \"directory\": \"%s\",\n", cwd);
        string_printf(allocator, result, "        \"command\": \"%s\",\n", cmd_str);
        string_printf(allocator, result, "        \"file\": \"%s\"\n", gen_compile_commands_json_escape(compile_cmd->src->path, temp_allocator));
        string_printf(allocator, result, "    }");
    }
    string_printf(allocator, result, "\n]");
    allocator_destroy(temp_allocator);
    return result;
}

static bool gen_compile_commands_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    char const* path = cmd->outputs[0]->path;
    Allocator* temp_allocator = allocator_create_chained();
    char* result = get_compile_commands_string(cmd, node_allocator);
    char* old_content = os_read_all(temp_allocator, path);
    bool b_dirty = false;
    if (old_content == NULL || !string_equal(old_content, result))
    {
        cmd->extra_data = result;
        b_dirty = true;
    }
    else
    {
        array_free(node_allocator, result);
    }
    allocator_destroy(temp_allocator);
    return b_dirty;
}

static int gen_compile_commands_thread_fn(Node* cmd)
{
    char const* path = cmd->outputs[0]->path;
    char const* compdb = NULL;
    if (cmd->extra_data)
    {
        compdb = cmd->extra_data;
    }
    else
    {
        compdb = get_compile_commands_string(cmd, node_allocator);
    }
    os_write_all(path, compdb, array_size(compdb));
    array_free(node_allocator, compdb);
    return EXIT_SUCCESS;
}

void set_generate_compile_commands_enabled(bool enabled)
{
    b_generate_compile_commands = enabled;
}

void set_compile_commands_json_path(char const* path)
{
    compile_commands_json_path = path;
}

ENTRY(gen_compile_commands_entry, PRIORITY_AFTER_DEFAULT)
{
    Node* output = get_or_add_file(compile_commands_json_path);
    output->b_default_excluded = !b_generate_compile_commands;
    Node* cmd = CALLBACK_CMD(gen_compile_commands_thread_fn, NULL);
    cmd->b_default_excluded = !b_generate_compile_commands;
    node_set_alias(cmd, "compdb");
    node_set_check_dirty_fn(cmd, gen_compile_commands_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{}{#}", output->path));

    Node** nodes = get_all_nodes();
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_VIRTUAL || node->virtual_ext_type != VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE)
        {
            continue;
        }
        node_add_dependency(cmd, node);
    }
}
