#include "core/allocator.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/fmt.h"
#include "cup/test.h"

static void test_compile_cup_h_impl(char const* toolchain, char const* cmdline, char const* file, int line)
{
    char const* src = fmt("{out_dir}/header_only/cup.h");
    if (!os_file_exists(src))
    {
        return;
    }
    char const* path = fmt("{out_dir}/tests/header_only_test_{}/cup.h", toolchain);
    char const* tests_dir = fmt("{out_dir}/tests");
    char const* dir = path_parent_path(path, allocator_temp());
    if (!string_starts_with(dir, tests_dir))
    {
        fprintf(stderr, "WARNING: refusing to delete path not under build/tests/: %s\n", dir);
        return;
    }
    else if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (os_file_exists(dir) && system(fmt("rmdir /s /q \"{}\" 2>NUL", dir)) != 0)
        {
            fprintf(stderr, "WARNING: rmdir failed for '%s'\n", dir);
        }
    }
    else
    {
        if (system(fmt("rm -rf \"{}\" 2>/dev/null", dir)) != 0)
        {
            fprintf(stderr, "WARNING: rm failed for '%s'\n", dir);
        }
    }
    os_ensure_dir_existed(path);
    os_copy_file(src, path);
    char const* cwd = os_get_cwd(allocator_temp());
    os_set_cwd(dir);
    assert_impl(system(cmdline) == 0, "compile failed", file, line);
    assert_impl(os_file_exists("cup" EXE_EXT), "cup not found", file, line);
    char const* run_cmd = CURRENT_PLATFORM == PLATFORM_WINDOWS ? "cup" EXE_EXT : "./cup";
    assert_impl(system(run_cmd) == 0, "run cup failed", file, line);
    assert_impl(os_file_exists("build.c"), "build.c not generated", file, line);
    os_set_cwd(cwd);
    if (os_file_exists(dir))
    {
        if (!string_starts_with(dir, tests_dir))
        {
            fprintf(stderr, "WARNING: refusing to delete path not under build/tests/: %s\n", dir);
        }
        else if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            if (system(fmt("rmdir /s /q \"{}\" 2>NUL", dir)) != 0)
            {
                fprintf(stderr, "WARNING: rmdir failed for '%s'\n", dir);
            }
        }
        else
        {
            if (system(fmt("rm -rf \"{}\" 2>/dev/null", dir)) != 0)
            {
                fprintf(stderr, "WARNING: rm failed for '%s'\n", dir);
            }
        }
    }
}

#define test_compile_cup_h(toolchain, cmdline) test_compile_cup_h_impl(toolchain, cmdline, __FILE__, __LINE__)

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
    test_compile_cup_h("clang", "clang -x c cup.h -DMAIN_ENTRY -o cup.exe");
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
    test_compile_cup_h("clang", "clang -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions");
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
    test_compile_cup_h("clang", "clang -x c cup.h -DMAIN_ENTRY -o cup -fms-extensions");
}

TEST(test_header_only_msvc_windows, header_only)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    bool msvc_set_arch_env(ArchitectureType type);
    msvc_set_arch_env(ARCH_X64);

    if (system("cl > NUL 2>&1") != 0)
    {
        return;
    }
    test_compile_cup_h("msvc", "cl /Fe:cup.exe /Tc cup.h /std:clatest /DMAIN_ENTRY /nologo");
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
    test_compile_cup_h("gcc", "gcc -x c cup.h -DMAIN_ENTRY -luserenv -lbcrypt -o cup.exe");
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
    test_compile_cup_h("gcc", "gcc -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions");
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
    test_compile_cup_h("gcc", "gcc -x c cup.h -DMAIN_ENTRY -o cup -fms-extensions");
}
