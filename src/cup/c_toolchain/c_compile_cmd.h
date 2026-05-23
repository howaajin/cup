#pragma once

#include "cup/c_toolchain/c_toolchain.h"
#include "cup/node.h"

typedef struct Node Node;
typedef struct StringPtrHash StringPtrHash;
typedef struct StringSet StringSet;
typedef struct CCompileCmd CCompileCmd;
typedef struct CompileCmdline CompileCmdline;
typedef struct ScanDepsCmd ScanDepsCmd;

struct CompileCmdline
{
    struct Node;
    CCompileCmd* cmd;
};

typedef struct CCompileCmd
{
    struct Node;
    Node* src;
    Node* out_obj;
    Node* pdb;
    ToolchainType toolchain;
    SourceType source_type;
    CLanguageStandard c_std;
    CppLanguageStandard cpp_std;
    OptimizationType optimization_type;
    ArchitectureType arch;
    StringSet* includes;
    StringSet* defines;
    char** flags;
    CompileCmdline* make_cmdline_cmd;
    bool b_cpp : 1;
    bool b_generate_debug_info : 1;
    bool b_cache_header_dependencies : 1;
    bool b_color_diagnostics : 1;
    bool b_scan_deps_dirty : 1;
    bool b_self_build : 1;
    bool b_import_std : 1;

    // Cpp module
    Node* export_bmi;
    char* export_name;
    ScanDepsCmd* scan_deps_cmd;
    char** import_names;
    Node** import_bmis;
    StringPtrHash* export_map;
    StringPtrHash* import_map;

    // GCC
    Node* module_mapper;

    // MSVC
    char const* input_filename;
    char const* cwd;
    size_t show_include_prefix_len;
    char const* msvc_show_include_prefix;

} CCompileCmd;

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line);
void c_compile_cmd_add_include_directory(Node* node, char const* dir);
void c_compile_cmd_add_define(Node* node, char const* define);
void c_compile_cmd_add_flag(Node* node, char const* flag);
void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std);
void c_compile_cmd_set_cpp_std(Node* cmd, CppLanguageStandard cpp_std);
void c_compile_cmd_set_arch(Node* cmd, ArchitectureType arch);
void c_compile_cmd_set_optimization_type(Node* cmd, OptimizationType type);
void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export_name(Node* node, char const* name);
void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
void c_compile_cmd_add_self_build_options(Node* node);
void c_compile_cmd_set_export_map(CCompileCmd* cmd, StringPtrHash* map);
void c_compile_cmd_set_import_map(CCompileCmd* cmd, StringPtrHash* map);
void compile_cmdline_node_make_cmdline(CompileCmdline* node);
