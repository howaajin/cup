#pragma once

#include "cup/fmt.h"

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct Allocator Allocator;
typedef struct Executor Executor;
typedef struct Graph Graph;
typedef struct Cache Cache;
typedef struct Node Node;
typedef struct Node File;
typedef struct Node Exe;
typedef struct Node Cmd;
typedef struct Node ExeCmd;
typedef struct Node ThreadCmd;
typedef struct Task Task;
typedef int FnThread(Node* node);
typedef bool FnCheckDirty(Node* node);
typedef void FnProcessed(Node* node, Graph* graph);
typedef void FnBeforeExecute(Node* node);
typedef void FnAfterExecute(Node* node);

typedef enum NodeType
{
    NODE_TYPE_VIRTUAL,
    NODE_TYPE_FILE,
    NODE_TYPE_CMD,
} NodeType;

typedef enum FileType
{
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_SRC = 1,
    FILE_TYPE_OBJ,
    FILE_TYPE_EXE,
    FILE_TYPE_DLL,
    FILE_TYPE_LIB,
    FILE_TYPE_ENV,
    FILE_TYPE_MODULE_MAPPER,
} FileType;

typedef enum CmdType
{
    CMD_TYPE_EXECUTABLE,
    CMD_TYPE_THREAD,
} CmdType;

typedef enum VirtualExtType
{
    VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE = 1,
} VirtualExtType;

typedef enum OptionType
{
    OPTION_NONE,
    OPTION_EXE = 1,
    OPTION_INPUT,
    OPTION_OUTPUT,
    OPTION_FLAG,
    OPTION_BRIGHT_FLAG,
    OPTION_HIDDEN,
} OptionType;

struct Node
{
    union
    {
        struct
        {
            NodeType node_type : 3;
            uint32_t virtual_ext_type : 29;
        };
        union
        {
            struct
            {
                NodeType : 3;
                FileType file_type : 5;
                uint32_t file_ext_type : 24;
            };
            struct
            {
                NodeType : 3;
                CmdType cmd_type : 3;
                uint32_t cmd_ext_type : 26;
            };
        };
        uint32_t type;
    };
    uint32_t indegree;
    Node** dependencies;
    void (*prepare)(Node* node);
    void (*visit)(Node* node, Graph* graph, Executor* executor); // Run when visiting the DAG node
    bool (*check_dirty)(Node* node); // Run when start visiting
    void (*processed)(Node* node, Graph* graph);
    union
    {
        char* name;
        char* path;
    };
    void* extra_data;
    void* ctx;
    union
    {
        struct // file
        {
            uint64_t mtime;
            uint64_t content_hash;
            Node* build_cmd;
            char const** debugger_run_arguments;
            char const** test_entries;
            wchar_t* env_block;
            bool b_path_checked : 1;
            bool b_has_backslash : 1;
            bool b_has_space : 1;
        };
        struct // command
        {
            union
            {
                struct // process
                {
                    char* cmdline;
                    char* extra_options;
                    void (*write_stdout_line_fn)(Node* cmd, char const* line);
                    void (*write_stderr_line_fn)(Node* cmd, char const* line);
                    char* std_output;
                    char* std_error;
                    char* stdout_line;
                    char* stderr_line;
                    Node* env_node;
                    Node* make_cmdline;
                };
                struct // thread
                {
                    FnThread* fn;
                };
            };
            void (*before_execute)(Node* node);
            void (*after_execute)(Node* node);
            Node** inputs;
            Node** outputs;
            Node** implicit_inputs2;
            char* description;
            int exit_code;
            char const* file;
            int line;
        };
    };
    bool b_default_excluded : 1;
    bool b_dynamic_indegree : 1;
    bool b_dirty : 1;
    bool b_prepared : 1;
};

typedef struct Obj
{
    struct Node;
    char const** link_libs;
    Node** link_nodes;
} Obj;

void init_node(void);

uint32_t node_make_file_type(FileType file_type, uint32_t ext_type);
uint32_t node_make_cmd_type(CmdType cmd_type, uint32_t ext_type);
uint32_t node_make_virtual_type(uint32_t ext_type);

bool node_virtual_check_dirty(Node* node);
void node_virtual_visit(Node* node, Graph* graph, Executor* executor);
void node_prepare(Node* node);
void node_visit(Node* node, Graph* graph, Executor* executor);
Node* node_create(uint32_t type, char const* name, size_t num_bytes);
void node_set_name(Node* node, char const* name);
void node_set_alias(Node* node, char const* alias);
void node_set_check_dirty_fn(Node* node, FnCheckDirty* fn);
void node_set_processed_fn(Node* node, FnProcessed* fn);
void node_set_extra_data(Node* node, void* extra_data);
void node_add_debugger_argument(Node* node, char const* arg);
void node_add_dependency(Node* node, Node* dependency);
void node_ensure_prepared(Node* node);

void cmd_prepare(Node* node);
void cmd_processed(Node* node, Graph* graph);
void cmd_visit(Node* node, Graph* graph, Executor* executor);
bool cmd_check_dirty(Node* node);
void cmd_before_execute(Node* node);
void cmd_update_output_mtime(Node* node);
void cmd_after_execute(Node* node);
void cmd_add_input(Node* node, Node* file);
void cmd_remove_input(Node* node, Node* file);
void cmd_add_output(Node* node, Node* file);
void cmd_remove_output(Node* node, Node* file);
void cmd_add_input_file_option(Node* node, char const* option, Node* file);
void cmd_add_output_file_option(Node* node, char const* option, Node* file);
void cmd_set_source_location(Node* node, char const* file, int line);
void cmd_write_stdout_line(Node* cmd, char const* line);
void cmd_write_stderr_line(Node* cmd, char const* line);
void cmd_set_write_output_line_fn(Node* node, void (*fn)(Node* cmd, char const* line));
void cmd_set_write_stderr_line_fn(Node* node, void (*fn)(Node* cmd, char const* line));
void cmd_set_before_execute_fn(Node* node, FnBeforeExecute* fn);
void cmd_set_after_execute_fn(Node* node, FnAfterExecute* fn);
void cmd_set_description(Node* node, char const* string);
void cmd_add_implicit_dep(Node* node, char const* dep);
void cmd_add_option(Node* node, char const* option, char const* param, OptionType type);
void cmd_set_env(Node* node, Node* env);
char const* cmd_get_description(Node* node);
char const* cmd_get_cmdline(Node* node);
Task* cmd_create_task(Node* cmd, Executor* executor);
Node* file_create(char const* path, size_t num_bytes);
void file_processed(Node* node, Graph* graph);
void file_visit(Node* node, Graph* graph, Executor* executor);
bool file_check_dirty(Node* node);
bool file_path_has_space(Node* node);
bool file_path_has_backslash(Node* node);
uint64_t file_get_content_hash(Node* node);
char const* file_get_option_path(Node* node);
Node* find_node(char const* name);
bool has_dependency(Node* node, Node* dependency);
Node** get_all_nodes(void);
Node* get_or_add_file(char const* path);
Node* get_or_add_src(char const* path);
Node* get_or_add_file_with_type(char const* path, FileType type);
Node* get_or_add_node(uint32_t type, char const* name, size_t num_bytes);
Node* add_thread_cmd(FnThread* fn, void* ctx, char const* file, int line);
Node* add_process_cmd(char const* string, char const* file, int line);
Node* add_process_cmd_from_exe_node(Node* exe, char const* name, char const* file, int line);

#define FILE(fmt_str, ...) get_or_add_file(fmt(fmt_str, ##__VA_ARGS__))
#define LIB(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{lib_ext}", ##__VA_ARGS__), FILE_TYPE_LIB)
#define EXE(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{exe_ext}", ##__VA_ARGS__), FILE_TYPE_EXE)
#define DLL(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{dll_ext}", ##__VA_ARGS__), FILE_TYPE_DLL)
#define CALLBACK_CMD(fn, ctx) add_thread_cmd(fn, ctx, __FILE__, __LINE__)
#define CMD(cmdline) add_process_cmd(cmdline, __FILE__, __LINE__)
#define CMD_FROM_EXE(node, name) add_process_cmd_from_exe_node(node, name, __FILE__, __LINE__)
