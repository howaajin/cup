#include "core/allocator.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "cup/fmt.h"
#include "cup/test.h"

static void test_binary_mode_impl(char const* path, char const* run_cmd, char const* file, int line)
{
    char const src[] = "build/embedded/cup" EXE_EXT;
    if (!os_file_exists(src))
    {
        return;
    }
    char const* dir = path_parent_path(path, allocator_temp());
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        system(fmt("rmdir /s /q \"{}\" 2>NUL", dir));
    }
    else
    {
        system(fmt("rm -rf \"{}\" 2>/dev/null", dir));
    }
    os_ensure_dir_existed(path);
    os_copy_file(src, path);
    char const* cwd = os_get_cwd(allocator_temp());
    os_set_cwd(dir);
    int result = system(run_cmd);
    bool build_c_exists = os_file_exists("build.c");
    bool dll_exists = os_file_exists("build/cup" DLL_EXT);
    os_set_cwd(cwd);
    assert_impl(result == 0, "run cup failed", file, line);
    assert_impl(build_c_exists, "build.c not generated", file, line);
    assert_impl(dll_exists, "build/cup" DLL_EXT " not generated", file, line);
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        system(fmt("rmdir /s /q \"{}\" 2>NUL", dir));
    }
    else
    {
        system(fmt("rm -rf \"{}\" 2>/dev/null", dir));
    }
}

#define test_binary_mode(path, run_cmd) test_binary_mode_impl(path, run_cmd, __FILE__, __LINE__)

TEST(test_binary_mode_clang_windows, binary_mode)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    if (system("clang --version > NUL 2>&1") != 0)
    {
        return;
    }
    test_binary_mode(
        "build/tests/binary_mode_test/cup.exe",
        "cup.exe");
}

TEST(test_binary_mode_clang_linux, binary_mode)
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
    test_binary_mode(
        "build/tests/binary_mode_test/cup",
        "./cup");
}

TEST(test_binary_mode_clang_mac, binary_mode)
{
    if (CURRENT_PLATFORM != PLATFORM_MACOS)
    {
        return;
    }
    if (system("clang --version > /dev/null 2>&1") != 0)
    {
        return;
    }
    test_binary_mode(
        "build/tests/binary_mode_test/cup",
        "./cup");
}
