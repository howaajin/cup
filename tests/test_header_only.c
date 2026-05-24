#include "core/allocator.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/test.h"

static void test_compile_cup_h_impl(char const* path, char const* cmdline, char const* file, int line)
{
    char const src[] = "build/header_only/cup.h";
    if (!os_file_exists(src))
    {
        return;
    }
    os_ensure_dir_existed(path);
    os_copy_file(src, path);
    char const* cwd = os_get_cwd(allocator_temp());
    char const* dir = path_parent_path(path, allocator_temp());
    os_set_cwd(dir);
    assert_impl(system(cmdline) == 0, "compile failed", file, line);
    assert_impl(os_file_exists("cup" EXE_EXT), "cup not found", file, line);
    assert_impl(system("./cup") == 0, "run cup failed", file, line);
    assert_impl(os_file_exists("build.c"), "build.c not generated", file, line);
    os_set_cwd(cwd);
}

#define test_compile_cup_h(path, cmdline) test_compile_cup_h_impl(path, cmdline, __FILE__, __LINE__)

TEST(test_header_only_clang_windows, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    if (system("clang --version > NUL 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_clang/cup.h",
        "clang -x c cup.h -DMAIN_ENTRY -o cup.exe");
    system("rmdir /s /q build\\tests\\header_only_test_clang");
}

TEST(test_header_only_clang_linux, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_LINUX)
    {
        return;
    }
    if (system("clang --version > /dev/null 2>&1") != 0)
    {
        fprintf(stderr, "clang not found, skip test clang");
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_clang/cup.h",
        "clang -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions");
    system("rm -rf build/tests/header_only_test_clang/");
}

TEST(test_header_only_clang_mac, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_MACOS)
    {
        return;
    }
    if (system("clang --version > /dev/null 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_clang/cup.h",
        "clang -x c cup.h -DMAIN_ENTRY -o cup -fms-extensions");
    system("rm -rf build/tests/header_only_test_clang/");
}

TEST(test_header_only_msvc_windows, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    bool msvc_set_arch_env(ArchitectureType type);
    msvc_set_arch_env(ARCH_X64);

    if (system("cl /? > NUL 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_msvc/cup.h",
        "cl /Fe:cup.exe /Tc cup.h /std:clatest /DMAIN_ENTRY /nologo");
    system("rmdir /s /q build\\tests\\header_only_test_msvc");
}

TEST(test_header_only_gcc_windows, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    if (system("gcc --version > NUL 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_gcc/cup.h",
        "gcc -x c cup.h -DMAIN_ENTRY -luserenv -lbcrypt -o cup.exe");
    system("rmdir /s /q build\\tests\\header_only_test_gcc");
}

TEST(test_header_only_gcc_linux, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_LINUX)
    {
        return;
    }
    if (system("gcc --version > /dev/null 2>&1") != 0)
    {
        fprintf(stderr, "gcc not found, skip test gcc");
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_gcc/cup.h",
        "gcc -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions");
    system("rm -rf build/tests/header_only_test_gcc/");
}

TEST(test_header_only_gcc_mac, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_MACOS)
    {
        return;
    }
    if (system("gcc --version > /dev/null 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h(
        "build/tests/header_only_test_gcc/cup.h",
        "gcc -x c cup.h -DMAIN_ENTRY -o cup -fms-extensions");
    system("rm -rf build/tests/header_only_test_gcc/");
}
