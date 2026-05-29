#pragma once

#include "cup/node.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Node Node;
typedef struct CCompileCmd CCompileCmd;
typedef struct StringPtrHash StringPtrHash;
typedef struct CompileCmdline CompileCmdline;

typedef enum ToolchainType
{
    TOOLCHAIN_TYPE_UNSPECIFIED = 0,
    TOOLCHAIN_TYPE_LLVM,
    TOOLCHAIN_TYPE_MSVC,
    TOOLCHAIN_TYPE_GCC,
    TOOLCHAIN_TYPE_ZIG,
    TOOLCHAIN_TYPE_TCC,
} ToolchainType;

static inline char const* get_toolchain_string(ToolchainType toolchain_type)
{
    switch (toolchain_type)
    {
    case TOOLCHAIN_TYPE_MSVC: return "msvc";
    case TOOLCHAIN_TYPE_LLVM: return "llvm";
    case TOOLCHAIN_TYPE_ZIG: return "zig";
    case TOOLCHAIN_TYPE_GCC: return "gcc";
    default: assert(false); return NULL;
    }
}

typedef enum ArchitectureType
{
    ARCH_UNSPECIFIED = 0,
    ARCH_X86 = 1,
    ARCH_X64 = 2,
    ARCH_ARM = 3,
    ARCH_ARM64 = 4,
    ARCH_COUNT,
} ArchitectureType;

static inline char const* get_arch_string(ArchitectureType arch)
{
    switch (arch)
    {
    case ARCH_X64: return "x64";
    case ARCH_X86: return "x86";
    case ARCH_ARM: return "arm";
    case ARCH_ARM64: return "arm64";
    default: return NULL;
    }
}

typedef enum OptimizationType
{
    OPTIMIZATION_TYPE_UNSPECIFIED = 0,
    OPTIMIZATION_TYPE_DEBUG,
    OPTIMIZATION_TYPE_RELEASE_FAST,
    OPTIMIZATION_TYPE_RELEASE_SMALL,
} OptimizationType;

static inline char const* get_optimization_string(OptimizationType optimization_type)
{
    switch (optimization_type)
    {
    case OPTIMIZATION_TYPE_DEBUG: return "debug";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "release_fast";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "release_small";
    case OPTIMIZATION_TYPE_UNSPECIFIED: return "release";
    default: return NULL;
    }
}

typedef enum CLanguageStandard
{
    C_LANGUAGE_STANDARD_UNSPECIFIED = 0,
    C_LANGUAGE_STANDARD_99,
    C_LANGUAGE_STANDARD_11,
    C_LANGUAGE_STANDARD_17,
    C_LANGUAGE_STANDARD_23,
} CLanguageStandard;

typedef enum CppLanguageStandard
{
    CPP_LANGUAGE_STANDARD_UNSPECIFIED = 0,
    CPP_LANGUAGE_STANDARD_98,
    CPP_LANGUAGE_STANDARD_11,
    CPP_LANGUAGE_STANDARD_14,
    CPP_LANGUAGE_STANDARD_17,
    CPP_LANGUAGE_STANDARD_20,
    CPP_LANGUAGE_STANDARD_23,
    CPP_LANGUAGE_STANDARD_COUNT,
} CppLanguageStandard;

static inline char const* get_cpp_std_string(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_98: return "cpp98";
    case CPP_LANGUAGE_STANDARD_11: return "cpp11";
    case CPP_LANGUAGE_STANDARD_14: return "cpp14";
    case CPP_LANGUAGE_STANDARD_17: return "cpp17";
    case CPP_LANGUAGE_STANDARD_20: return "cpp20";
    case CPP_LANGUAGE_STANDARD_23: return "cpp23";
    default: return NULL;
    }
}

typedef enum LinkerType
{
    LINKER_UNSPECIFIED,
    LINKER_LINK,
    LINKER_LD,
    LINKER_LLVM_LINK,
    LINKER_LLVM_LD,
    LINKER_LLVM_LLD,
    LINKER_ZIG_CC,
} LinkerType;

typedef enum SourceType
{
    SOURCE_TYPE_UNKNOWN,
    SOURCE_TYPE_C,
    SOURCE_TYPE_CPP,
    SOURCE_TYPE_CPPM,
} SourceType;

typedef struct Src
{
    struct Node;
    SourceType source_type;
    Obj* default_obj;
} Src;

typedef struct TestExe
{
    struct Node;
    char const** entries;
} TestExe;

typedef struct BmiToObjCmd
{
    struct Node;
    CCompileCmd* c_compile_cmd;
} BmiToObjCmd;

void set_default_toolchain(ToolchainType type);
ToolchainType get_toolchain_by_current_compiler();
ToolchainType get_default_toolchain(void);
void set_llvm_linker_type(LinkerType type);
LinkerType get_llvm_linker_type(void);
void set_default_architecture(ArchitectureType type);
ArchitectureType get_default_architecture(void);
void set_default_optimization(OptimizationType type);
OptimizationType get_default_optimization(void);
void set_default_c_std(CLanguageStandard std);
CLanguageStandard get_default_c_std(void);
void set_default_cpp_std(CppLanguageStandard std);
CppLanguageStandard get_default_cpp_std(void);
void set_self_build_toolchain(ToolchainType toolchain);
void set_generate_vs_projects_enabled(bool enabled);
void set_generate_vscode_files_enabled(bool enabled);
void set_debug_info_enabled(bool b_enabled);
void set_test_enabled(bool b_enabled);
void set_msvc_show_include_prefix(char const* prefix);
void set_zig_target(char const* target);
void add_build_script(char const* path);
void add_build_script_search_directory(char const* directory);
Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch);
bool is_test_exe(Node* exe);
char const** get_test_entries(Node* exe);
Node* module_from_src(Node* src);

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line);
void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std);
void c_compile_cmd_add_include_directory(Node* node, char const* dir);
void c_compile_cmd_add_define(Node* node, char const* define);
void c_compile_cmd_add_flag(Node* node, char const* flag);
void c_compile_cmd_set_arch(Node* cmd, ArchitectureType arch);
void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export_name(Node* node, char const* name);
void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
void c_compile_cmd_add_self_build_options(Node* node);

// Obj
Obj* obj_create(char const* path);
Node* obj_from_src(Node* src);
void obj_add_link_obj_from_src(Node* node, Node* src);
void obj_add_link_node(Node* node, Node* other);
void obj_add_link_lib(Node* node, char const* lib);
CCompileCmd* obj_get_compile_cmd(Node* obj);
char** obj_get_compile_includes(Node* obj, Allocator* allocator);
Node* get_default_obj(Node* src);

Node* link_cmd_create(Node* output, char const* file, int line);
void link_cmd_add_input(Node* cmd, Node* file);
void link_cmd_set_pdb(Node* cmd, Node* pdb);
Node* link_cmd_set_pdb_base_on_output(Node* cmd);
void link_cmd_set_out_import_lib(Node* cmd, Node* out_import_lib);
Node* link_cmd_get_out_import_lib(Node* cmd);
void link_cmd_set_def_file(Node* cmd, Node* def);
void link_cmd_add_lib(Node* cmd, char const* lib);
void link_cmd_add_lib_dir(Node* cmd, char const* directory);
void link_cmd_add_flag(Node* cmd, char const* flag);
void link_cmd_set_entry(Node* cmd, char const* name);
void link_cmd_set_arch(Node* cmd, ArchitectureType arch);
void link_cmd_set_toolchain_type(Node* cmd, ToolchainType toolchain_type);
void link_cmd_set_linker_type(Node* cmd, LinkerType linker_type);
void link_cmd_setup_self_build(Node* cmd);

Node* ar_cmd_create(Node* output, char const* file, int line);
void ar_cmd_add_input(Node* node, Node* input);
void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain);
Node* make_implib_cmd_create(Node* output, Node* def, ToolchainType toolchain_type, ArchitectureType architecture_type, char const* src_file_path, int line);

Node* get_or_add_src(char const* path);

#define LINK(output) link_cmd_create(output, __FILE__, __LINE__)
#define OBJ(src) get_default_obj(src)
#define SRC(fmt_str, ...) get_or_add_src(fmt(fmt_str, ##__VA_ARGS__))
#define CC(input, output) c_compile_cmd_create(input, output, __FILE__, __LINE__)
#define AR(output) ar_cmd_create(output, __FILE__, __LINE__)
