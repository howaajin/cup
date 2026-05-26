#include "cup/c_toolchain/scan_deps_cmd.h"

#include "core/json.h"
#include "cup/cup.h"
#include "cup/executor/executor.h"

#include "cup/test.h"

void verify_p1689(ScanDepsCmd* scan)
{
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 4);
    Task* task = cmd_create_task((Node*)scan, executor);
    executor_add_task(executor, task);
    Task* finished = executor_wait(executor);
    ASSERT(task == finished);
    ASSERT(task->exit_code == EXIT_SUCCESS);
    scan->after_execute((Node*)scan);
    ASSERT(string_equal(scan->export_name, "module1"));
    ASSERT(array_size(scan->imports) == 1);
    ASSERT(string_equal(scan->imports[0], "module2"));
    allocator_destroy(allocator);
}

TEST(test_scan_deps_cmd_llvm, scan_deps_cmd)
{
    if (system("clang --version") != 0)
    {
        return;
    }
    char const src_path[] = "tests/cpp_module/module1.cppm";
    Node* src = SRC(src_path);
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    CCompileCmd* compile_cmd = (CCompileCmd*)cc;
    compile_cmd->toolchain = TOOLCHAIN_TYPE_LLVM;
    compile_cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    c_compile_cmd_add_define(cc, "SCAN_DEPS_XXX");
    c_compile_cmd_add_include_directory(cc, "scan_inc");
    c_compile_cmd_add_flag(cc, "-g");
    ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmd_create(compile_cmd);
    ASSERT(string_contains(scan->cmdline, "clang-scan-deps"));
    ASSERT(string_contains(scan->cmdline, " --format p1689"));
    ASSERT(string_contains(scan->cmdline, " -- "));
    ASSERT(string_contains(scan->cmdline, " -std=c++20"));
    ASSERT(string_contains(scan->cmdline, src_path));
    ASSERT(string_contains(scan->cmdline, " -DSCAN_DEPS_XXX"));
    ASSERT(string_contains(scan->cmdline, " -Iscan_inc"));
    ASSERT(string_contains(scan->cmdline, " -g"));
    verify_p1689(scan);
}

TEST(test_scan_deps_cmd_msvc, scan_deps_cmd)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    char const src_path[] = "tests/cpp_module/module1.cppm";
    Node* src = SRC(src_path);
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    CCompileCmd* compile_cmd = (CCompileCmd*)cc;
    compile_cmd->toolchain = TOOLCHAIN_TYPE_MSVC;
    compile_cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    c_compile_cmd_add_define(cc, "SCAN_DEPS_XXX");
    c_compile_cmd_add_include_directory(cc, "scan_inc");
    c_compile_cmd_add_flag(cc, "/Zi");
    ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmd_create(compile_cmd);
    compile_cmd->prepare(cc);
    ASSERT(string_contains(scan->cmdline, "cl"));
    ASSERT(string_contains(scan->cmdline, " /scanDependencies"));
    ASSERT(string_contains(scan->cmdline, " /std:c++20"));
    ASSERT(string_contains(scan->cmdline, src_path));
    ASSERT(string_contains(scan->cmdline, " /DSCAN_DEPS_XXX"));
    ASSERT(string_contains(scan->cmdline, " /Iscan_inc"));
    ASSERT(string_contains(scan->cmdline, " /TP"));
    ASSERT(string_contains(scan->cmdline, " /Zi"));
    verify_p1689(scan);
}

TEST(test_scan_deps_cmd_gcc, scan_deps_cmd)
{
    if (system("g++ --version") != 0)
    {
        return;
    }
    char const src_path[] = "tests/cpp_module/module1.cppm";
    Node* src = SRC(src_path);
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    CCompileCmd* compile_cmd = (CCompileCmd*)cc;
    compile_cmd->toolchain = TOOLCHAIN_TYPE_GCC;
    compile_cmd->cpp_std = CPP_LANGUAGE_STANDARD_20;
    c_compile_cmd_add_define(cc, "SCAN_DEPS_XXX");
    c_compile_cmd_add_include_directory(cc, "scan_inc");
    c_compile_cmd_add_flag(cc, "-g");
    ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmd_create(compile_cmd);
    ASSERT(string_contains(scan->cmdline, "g++"));
    ASSERT(string_contains(scan->cmdline, " -fdeps-format=p1689r5"));
    ASSERT(string_contains(scan->cmdline, " -MM"));
    ASSERT(string_contains(scan->cmdline, " -std=c++20"));
    ASSERT(string_contains(scan->cmdline, src_path));
    ASSERT(string_contains(scan->cmdline, " -DSCAN_DEPS_XXX"));
    ASSERT(string_contains(scan->cmdline, " -Iscan_inc"));
    ASSERT(string_contains(scan->cmdline, " -g"));
    verify_p1689(scan);
}