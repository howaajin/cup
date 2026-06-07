#include "cup/c_toolchain/scan_test.h"
#include "core/array.h"
#include "core/string.h"
#include "core/utilities.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/cache.h"
#include "cup/fmt.h"
#include "cup/node.h"

typedef struct ScanTestCmd ScanTestCmd;

static int scan_test_cmd_thread_fn(Node* node)
{
    char** test_finder_get_entries(Allocator * allocator, char const* src_path);
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    // node_allocator cannot be used in a thread
    cmd->entries = test_finder_get_entries(allocator_c(), cmd->src->path);
    return 0;
}

static void scan_test_update_cache(ScanTestCmd* cmd)
{
    Cache* cache = get_cache();
    if (cache == NULL || cmd->entries == NULL)
    {
        return;
    }
    CacheRecordTestExe* record = cache_find_test_exe(cache, cmd->compile_cmd->src->path);
    if (!record)
    {
        goto WriteNew;
    }
    size_t num_record_entries = array_size(record->entries);
    if (num_record_entries != array_size(cmd->entries))
    {
        goto WriteNew;
    }
    for (size_t i = 0; i != num_record_entries; i++)
    {
        if (!string_equal(record->entries[i], cmd->entries[i]))
        {
            goto WriteNew;
        }
    }
    return;
WriteNew:; // tcc: label must be followed by a statement, not a declaration
    CacheRecordFile* src_record = cache_get_or_add_in_file_record(cache, cmd->src->path);
    CacheRecordTestExe new_record = {
        .source_id = src_record->id,
        .entries = cmd->entries,
    };
    cache_write_test_exe_record(cache, &new_record);
}

extern Allocator* node_allocator;

static void scan_test_after_execute(Node* node)
{
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    Node* src = cmd->src;
    scan_test_update_cache(cmd);
    src->test_entries = (char const**)utilities_copy_string_array(node_allocator, (char const**)cmd->entries);
    array_free(allocator_c(), cmd->entries);
    cmd->entries = NULL;
    cmd_after_execute(node);
}

Node* scan_test_cmd_create(CCompileCmd* compile_cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_THREAD, C_CMD_SCAN_TESTS);
    char const* name = fmt("scan_test: {:n}", compile_cmd->src);
    Node* node = find_node(name);
    if (node)
    {
        return node;
    }
    node = node_create(type, name, sizeof(ScanTestCmd));
    node->fn = scan_test_cmd_thread_fn;
    node->after_execute = scan_test_after_execute;
    node->b_default_excluded = true;
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    cmd->src = compile_cmd->src;
    cmd->compile_cmd = compile_cmd;
    cmd_add_input(node, compile_cmd->src);
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordTestExe* test_record = cache_find_test_exe(cache, cmd->src->path);
        if (test_record)
        {
            cmd->src->test_entries = (char const**)test_record->entries;
        }
    }
    return node;
}

void get_scan_test_cmds(Allocator* allocator, Node*** out_cmds)
{
    extern Node** nodes;
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
        if (cc->src->build_cmd)
        {
            continue;
        }
        Node* scan = scan_test_cmd_create(cc);
        node_ensure_prepared(scan);
        array_push(allocator, *out_cmds, scan);
    }
}
