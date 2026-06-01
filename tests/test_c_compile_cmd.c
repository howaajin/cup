#include "core/platform.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/cpp_module.h"
#include "cup/cache.h"
#include "cup/node.h"
#include "cup/test.h"

TEST(test_c_compile_cmd_gcc_c, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_GCC);

    Node* src = SRC("fake.c");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_C);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->c_std = C_LANGUAGE_STANDARD_17;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "gcc "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF "));
    ASSERT(string_contains(cmd->extra_options, "-fdiagnostics-color"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "-std=c17"));
    ASSERT(string_contains(cmd->cmdline, "-O0"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
}

TEST(test_c_compile_cmd_gcc_cpp, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_GCC);

    Node* src = SRC("fake.cpp");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPP);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "g++ "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF "));
    ASSERT(string_contains(cmd->extra_options, "-fdiagnostics-color"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "-std=c++20"));
    ASSERT(string_contains(cmd->cmdline, "-O0"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
}

TEST(test_c_compile_cmd_gcc_cppm, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_GCC);

    Node* src = SRC("fake.cpp");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPP);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;

    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_import(node, "std", NULL);
    c_compile_cmd_set_export_name(node, "fake");

    cmd->prepare(node);

    Node* std_module = get_or_create_std_module_for_compile_cmd(cmd);
    ASSERT(std_module);

    ASSERT(cmd->export_bmi);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->module_mapper));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));
    ASSERT(has_dependency(cmd->export_bmi, node));
    ASSERT(has_dependency(node, std_module));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "g++ "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF "));
    ASSERT(string_contains(cmd->extra_options, "-fdiagnostics-color"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-std=c++20"));
    ASSERT(string_contains(cmd->cmdline, "-O0"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
    ASSERT(string_contains(cmd->cmdline, "-fmodules"));
    ASSERT(string_contains(cmd->cmdline, "-fmodule-mapper=build/mappers"));
}

TEST(test_c_compile_cmd_llvm_c, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);

    Node* src = SRC("fake.c");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_C);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->c_std = C_LANGUAGE_STANDARD_17;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "clang "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF "));
    ASSERT(string_contains(cmd->extra_options, "-fcolor-diagnostics"));
    ASSERT(string_contains(cmd->extra_options, "-fansi-escape-codes"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "-std=c17"));
    ASSERT(string_contains(cmd->cmdline, "-O0"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
}

TEST(test_c_compile_cmd_llvm_cpp, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);

    Node* src = SRC("fake.cpp");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPP);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "clang++ "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF "));
    ASSERT(string_contains(cmd->extra_options, "-fcolor-diagnostics"));
    ASSERT(string_contains(cmd->extra_options, "-fansi-escape-codes"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "-std=c++20"));
    ASSERT(string_contains(cmd->cmdline, "-O0"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
}

TEST(test_c_compile_cmd_llvm_cppm, c_compile_cmd)
{
    if (system("clang --version") != 0)
    {
        return;
    }
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);

    Node* src = SRC("fake.ixx");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPPM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_23;
    cmd->optimization_type = OPTIMIZATION_TYPE_RELEASE_FAST;
    cmd->arch = ARCH_X86;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "NDEBUG");
    c_compile_cmd_add_flag(node, "-v");
    c_compile_cmd_add_import(node, "std", NULL);

    cmd->prepare(node);

    Node* std_module = get_or_create_std_module_for_compile_cmd(cmd);
    ASSERT(std_module);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->export_bmi, node));
    ASSERT(has_dependency(node, std_module));

    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);

    // Cmdline
    ASSERT(string_starts_with(cmd->cmdline, "clang++ "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->extra_options, "-MMD -MF")); // with cmd->scan_deps_cmd != NULL
    ASSERT(string_contains(cmd->extra_options, "-fcolor-diagnostics"));
    ASSERT(string_contains(cmd->extra_options, "-fansi-escape-codes"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DNDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-v"));
    ASSERT(string_contains(cmd->cmdline, "-std=c++23"));
    ASSERT(string_contains(cmd->cmdline, "-O3"));
    ASSERT(string_contains(cmd->cmdline, "-m32"));
    ASSERT(string_contains(cmd->cmdline, "-fmodule-file=std"));
}

TEST(test_c_compile_cmd_msvc_c, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_MSVC);

    Node* src = SRC("fake.c");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_C);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->c_std = C_LANGUAGE_STANDARD_17;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "cl "));
    ASSERT(string_contains(cmd->cmdline, "/Zi"));
    ASSERT(string_contains(cmd->extra_options, "/showIncludes"));
    ASSERT(string_contains(cmd->cmdline, "/Isrc"));
    ASSERT(string_contains(cmd->cmdline, "/DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "/std:c17"));
    ASSERT(string_contains(cmd->cmdline, "/Od"));
}

TEST(test_c_compile_cmd_msvc_cpp, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_MSVC);

    Node* src = SRC("fake.cpp");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPP);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "cl "));
    ASSERT(string_contains(cmd->cmdline, "/Zi"));
    ASSERT(string_contains(cmd->extra_options, "/showIncludes"));
    ASSERT(string_contains(cmd->cmdline, "/Isrc"));
    ASSERT(string_contains(cmd->cmdline, "/DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "/std:c++20"));
    ASSERT(string_contains(cmd->cmdline, "/Od"));
}

TEST(test_c_compile_cmd_msvc_cppm, c_compile_cmd)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    set_default_toolchain(TOOLCHAIN_TYPE_MSVC);

    Node* src = SRC("fake.cppm");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_CPPM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_import(node, "std", NULL);

    cmd->prepare(node);

    Node* std_module = get_or_create_std_module_for_compile_cmd(cmd);
    ASSERT(std_module);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));
    ASSERT(has_dependency(cmd->export_bmi, node));
    ASSERT(has_dependency(cmd->pdb, node));
    ASSERT(has_dependency(node, std_module));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "cl "));
    ASSERT(string_contains(cmd->cmdline, "/Zi"));
    ASSERT(string_contains(cmd->extra_options, "/showIncludes"));
    ASSERT(string_contains(cmd->cmdline, "/Isrc"));
    ASSERT(string_contains(cmd->cmdline, "/DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "/std:c++20"));
    ASSERT(string_contains(cmd->cmdline, "/Od"));
    ASSERT(string_contains(cmd->cmdline, "/ifcOutput"));
    ASSERT(string_contains(cmd->cmdline, "/TP"));
    ASSERT(string_contains(cmd->cmdline, "/reference std="));
}

TEST(test_c_compile_cmd_gcc_asm, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_GCC);

    Node* src = SRC("fake.s");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_ASM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "DEBUG");
    c_compile_cmd_add_flag(node, "-xxx");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "gcc "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DDEBUG"));
    ASSERT(string_contains(cmd->cmdline, "-xxx"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
    // Pure .s asm: no optimization, no color diagnostics, no dep tracking
    ASSERT(!string_contains(cmd->cmdline, "-O"));
    ASSERT(!string_contains(cmd->extra_options, "-fdiagnostics-color"));
    ASSERT(!string_contains(cmd->cmdline, "-MMD"));
    ASSERT(!string_contains(cmd->cmdline, "-MF"));
    // No C/C++ std flags for asm
    ASSERT(!string_contains(cmd->cmdline, "-std=c"));
    ASSERT(!string_contains(cmd->cmdline, "-std=c++"));
}

TEST(test_c_compile_cmd_llvm_asm, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);

    Node* src = SRC("fake.s");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_ASM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "src");
    c_compile_cmd_add_define(node, "ASM_DEFINE");
    c_compile_cmd_add_flag(node, "-Wa,-alh");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "clang "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->cmdline, "-Isrc"));
    ASSERT(string_contains(cmd->cmdline, "-DASM_DEFINE"));
    ASSERT(string_contains(cmd->cmdline, "-Wa,-alh"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
    // Pure .s asm: no optimization, no color diagnostics, no dep tracking
    ASSERT(!string_contains(cmd->cmdline, "-O"));
    ASSERT(!string_contains(cmd->extra_options, "-fcolor-diagnostics"));
    ASSERT(!string_contains(cmd->extra_options, "-fansi-escape-codes"));
    ASSERT(!string_contains(cmd->cmdline, "-MMD"));
    ASSERT(!string_contains(cmd->cmdline, "-MF"));
    // No C/C++ std flags for asm
    ASSERT(!string_contains(cmd->cmdline, "-std=c"));
    ASSERT(!string_contains(cmd->cmdline, "-std=c++"));
}

TEST(test_c_compile_cmd_zigcc_asm, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_ZIG);

    Node* src = SRC("fake.s");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_ASM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->optimization_type = OPTIMIZATION_TYPE_RELEASE_FAST;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "include");
    c_compile_cmd_add_define(node, "FEATURE=1");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "zig cc "));
    ASSERT(string_contains(cmd->cmdline, "-g"));
    ASSERT(string_contains(cmd->cmdline, "-Iinclude"));
    ASSERT(string_contains(cmd->cmdline, "-DFEATURE=1"));
    ASSERT(string_contains(cmd->cmdline, "-m64"));
    // Pure .s asm: no optimization, no color diagnostics, no dep tracking
    ASSERT(!string_contains(cmd->cmdline, "-O"));
    ASSERT(!string_contains(cmd->extra_options, "-fcolor-diagnostics"));
    ASSERT(!string_contains(cmd->extra_options, "-fansi-escape-codes"));
    ASSERT(!string_contains(cmd->extra_options, "-fdiagnostics-color=always"));
    ASSERT(!string_contains(cmd->cmdline, "-MMD"));
    ASSERT(!string_contains(cmd->cmdline, "-MF"));
    // No C/C++ std flags for asm
    ASSERT(!string_contains(cmd->cmdline, "-std=c"));
    ASSERT(!string_contains(cmd->cmdline, "-std=c++"));
}

TEST(test_c_compile_cmd_msvc_asm, c_compile_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_MSVC);

    Node* src = SRC("fake.asm");
    Node* obj = get_default_obj(src);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);

    CCompileCmd* cmd = (CCompileCmd*)node;

    ASSERT(cmd->source_type == SOURCE_TYPE_ASM);

    cmd->b_generate_debug_info = true;
    cmd->b_cache_header_dependencies = true;
    cmd->b_color_diagnostics = true;
    cmd->optimization_type = OPTIMIZATION_TYPE_DEBUG;
    cmd->arch = ARCH_X64;
    c_compile_cmd_add_include_directory(node, "include");
    c_compile_cmd_add_define(node, "WIN64");

    cmd->prepare(node);

    // Edges
    ASSERT(has_dependency(node, cmd->src));
    ASSERT(has_dependency(node, cmd->make_cmdline));
    ASSERT(has_dependency(cmd->out_obj, node));

    // Cmdline
    CompileCmdline* compile_cmdline_node = (CompileCmdline*)cmd->make_cmdline;
    compile_cmdline_node_make_cmdline(compile_cmdline_node);
    ASSERT(string_starts_with(cmd->cmdline, "ml64 "));
    ASSERT(string_contains(cmd->cmdline, "/nologo"));
    ASSERT(string_contains(cmd->cmdline, "/Zi"));
    ASSERT(string_contains(cmd->cmdline, "/Iinclude"));
    ASSERT(string_contains(cmd->cmdline, "/DWIN64"));
}