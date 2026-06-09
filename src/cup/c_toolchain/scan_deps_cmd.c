#include "cup/c_toolchain/scan_deps_cmd.h"
#include "core/json.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/cache.h"

#include "core/array.h"

extern Allocator* node_allocator;
void c_compile_cmd_init_msvc(CCompileCmd* cmd);

Node** scan_deps_cmds = NULL;

static StringPtrHash* get_default_module_mapper()
{
    static StringPtrHash* module_mapper = NULL;
    if (module_mapper == NULL)
    {
        module_mapper = allocator_calloc(node_allocator, 1, sizeof(StringPtrHash));
        expect(module_mapper, "allocation failed");
        module_mapper->allocator = node_allocator;
    }
    return module_mapper;
}

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_msvc_scan_deps_common(Node* node, CCompileCmd* cmd);
char const* c_compile_cmd_get_depfile_path(CCompileCmd* cmd);
void c_compile_cmd_write_buffer_msvc(Node* node, char const* line);

void scan_deps_cmd_add_option_mm_mf(Node* node, ScanDepsCmd* cmd)
{
    char const* depfile_path = c_compile_cmd_get_depfile_path(cmd->compile_cmd);
    cmd_add_option(node, OPTION_FLAG, fmt("-MM -MF {}", depfile_path));
}

static void scan_deps_cmd_make_cmdline_llvm(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, OPTION_EXE, "clang-scan-deps");
    cmd_add_option(node, OPTION_FLAG, "--format p1689");
    cmd_add_option(node, OPTION_FLAG, "-- clang++");
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd->compile_cmd);
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        scan_deps_cmd_add_option_mm_mf(node, cmd);
    }
}

static void scan_deps_cmd_make_cmdline_msvc(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, OPTION_EXE, "cl");
    cmd_add_option(node, OPTION_FLAG, "/scanDependencies-");
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        cmd_add_option(node, OPTION_FLAG, "/showIncludes");
    }
    compile_cmdline_node_make_cmdline_msvc_scan_deps_common(node, cmd->compile_cmd);
}

static void scan_deps_cmd_make_cmdline_gcc(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, OPTION_EXE, "g++");
    cmd_add_option(node, OPTION_FLAG, "-fmodules");
    cmd_add_option(node, OPTION_FLAG, "-E");
    cmd_add_option(node, OPTION_FLAG, "-fdirectives-only");
    cmd_add_option(node, OPTION_FLAG, "-fdeps-format=p1689r5");
    cmd_add_option(node, OPTION_FLAG, "-fdeps-file=-");
    cmd_add_option(node, OPTION_FLAG, "-MM");
    cmd_add_option(node, OPTION_FLAG, "-MG");
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd->compile_cmd);
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        scan_deps_cmd_add_option_mm_mf(node, cmd);
    }
}

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);

static bool scan_deps_cmd_read_p1689(ScanDepsCmd* cmd)
{
    Allocator* temp_allocator = allocator_temp();
    JsonValue json = json_from_string(cmd->std_output, temp_allocator);
    if (json.type != JSON_TYPE_OBJECT)
    {
        return false;
    }
    JsonValue* rules = json_object_get_value(&json.object, "rules");
    if (rules == NULL)
    {
        return false;
    }
    if (array_size(rules->array) == 0)
    {
        return false;
    }
    cmd->export_name = NULL;
    cmd->imports = NULL;
    JsonValue* rules0 = &rules->array[0];
    JsonValue* provides = json_object_get_value(&rules0->object, "provides");
    if (provides)
    {
        JsonValue* provides1 = &provides->array[0];
        JsonValue* logical_name = json_object_get_value(&provides1->object, "logical-name");
        cmd->export_name = string_from_c_str(node_allocator, logical_name->string);
    }

    JsonValue* _requires = json_object_get_value(&rules0->object, "requires");
    if (_requires)
    {
        size_t num_requires = array_size(_requires->array);
        for (size_t i = 0; i != num_requires; i++)
        {
            JsonValue* req = &_requires->array[i];
            JsonValue* req_name = json_object_get_value(&req->object, "logical-name");
            char* import_name = string_from_c_str(node_allocator, req_name->string);
            array_push(node_allocator, cmd->imports, import_name);
        }
    }
    return true;
}

static void scan_deps_cmd_update_cache(ScanDepsCmd* cmd)
{
    Cache* cache = get_cache();
    if (cache == NULL || (cmd->imports == NULL && cmd->export_name == NULL))
    {
        return;
    }
    Node* src = cmd->compile_cmd->src;
    CacheRecordCppModule* record = cache_find_cpp_module_record(cache, src->path);
    if (!record)
    {
        goto WriteNew;
    }
    size_t num_record_entries = array_size(record->imports);
    if (num_record_entries != array_size(cmd->imports))
    {
        goto WriteNew;
    }
    for (size_t i = 0; i != num_record_entries; i++)
    {
        if (!string_equal(record->imports[i], cmd->imports[i]))
        {
            goto WriteNew;
        }
    }
    if (array_size(cmd->export_name) != array_size(record->export))
    {
        goto WriteNew;
    }
    if (memcmp(cmd->export_name, record->export, array_size(record->export)) != 0)
    {
        goto WriteNew;
    }
    return;
WriteNew:; // tcc: label must be followed by a statement, not a declaration
    CacheRecordFile* src_record = cache_get_or_add_in_file_record(cache, src->path);
    CacheRecordCppModule new_record = {
        .source_id = src_record->id,
        .export = cmd->export_name,
        .imports = cmd->imports,
    };
    cache_write_cpp_module_record(cache, &new_record);
}

static void scan_deps_cmd_after_execute(Node* node)
{
    ScanDepsCmd* cmd = (ScanDepsCmd*)node;
    if (!scan_deps_cmd_read_p1689(cmd))
    {
        warn("%s failed!", fmt("{:n}", cmd));
    }
    array_free(node_allocator, cmd->std_output);
    cmd_update_output_mtime(node);
    scan_deps_cmd_update_cache(cmd);
    cmd_after_execute(node);
}

void scan_deps_cmd_llvm_gcc_setup_execute_callback(Node* node, ScanDepsCmd* cmd)
{
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        cmd_set_out_depfile(node, FILE("{}.d", cmd->compile_cmd->out_obj->path));
    }
    node->after_execute = scan_deps_cmd_after_execute;
}

Node* scan_deps_cmd_create(CCompileCmd* compile_cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_SCAN_DEPS);
    char const* name = fmt("scan deps: {:n}", compile_cmd->src);
    Node* node = node_create(type, name, sizeof(ScanDepsCmd));
    array_push(node_allocator, scan_deps_cmds, node);
    ScanDepsCmd* cmd = (ScanDepsCmd*)node;
    cmd->compile_cmd = compile_cmd;
    compile_cmd->scan_deps_cmd = cmd;
    compile_cmd->export_map = get_default_module_mapper();
    compile_cmd->import_map = get_default_module_mapper();
    Node* test_h = FILE("{test_src_dir}/cup/test.h");
    if (has_dependency((Node*)cmd->compile_cmd, test_h))
    {
        node_add_dependency(node, test_h);
    }
    cmd_add_input(node, compile_cmd->src);
    cmd_set_source_location(node, compile_cmd->file, compile_cmd->line);
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordCppModule* record = cache_find_cpp_module_record(cache, compile_cmd->src->path);
        if (record)
        {
            cmd->export_name = record->export;
            cmd->imports = record->imports;
        }
    }
    switch (compile_cmd->toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC:
        c_compile_cmd_init_msvc(compile_cmd);
        cmd_set_env(node, msvc_get_env_node(compile_cmd->toolchain, compile_cmd->arch));
        node->ctx = compile_cmd;
        node->write_stderr_line_fn = c_compile_cmd_write_buffer_msvc;
        scan_deps_cmd_make_cmdline_msvc(node, cmd);
        node->after_execute = scan_deps_cmd_after_execute;
        break;
    case TOOLCHAIN_TYPE_LLVM:
        scan_deps_cmd_make_cmdline_llvm(node, cmd);
        scan_deps_cmd_llvm_gcc_setup_execute_callback(node, cmd);
        break;
    case TOOLCHAIN_TYPE_GCC:
        scan_deps_cmd_make_cmdline_gcc(node, cmd);
        scan_deps_cmd_llvm_gcc_setup_execute_callback(node, cmd);
        break;
    case TOOLCHAIN_TYPE_ZIG:
        error("scan_deps_cmd_create: zig c++ do not support c++ modules");
        exit(EXIT_FAILURE);
    default:
        error("scan_deps_cmd_create: unknown toolchain");
        exit(EXIT_FAILURE);
    }
    return node;
}