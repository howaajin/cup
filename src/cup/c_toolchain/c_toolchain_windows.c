#include "core/directory.h"
#include "core/dylib.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/fmt.h"
#include "cup/node.h"
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <assert.h>
#include <userenv.h>

#pragma comment(lib, "Userenv.lib")

struct RegAPI
{
    long(WINAPI* RegOpenKeyExA)(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
    long(WINAPI* RegEnumKeyExA)(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime);
    long(WINAPI* RegCloseKey)(HKEY hKey);
    long(WINAPI* RegQueryValueExA)(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
};

static struct RegAPI* msvc_get_reg_api()
{
    static struct RegAPI RegAPI;
    if (RegAPI.RegOpenKeyExA == NULL)
    {
        Dylib* advapi32_dll = dylib_load("Advapi32.dll");
        if (advapi32_dll == NULL)
        {
            error("CToolchain_init_arch failed: can not found Advapi32.dll");
            exit(EXIT_FAILURE);
        }
        else
        {
            RegAPI.RegOpenKeyExA = dylib_get_symbol(advapi32_dll, "RegOpenKeyExA");
            RegAPI.RegEnumKeyExA = dylib_get_symbol(advapi32_dll, "RegEnumKeyExA");
            RegAPI.RegCloseKey = dylib_get_symbol(advapi32_dll, "RegCloseKey");
            RegAPI.RegQueryValueExA = dylib_get_symbol(advapi32_dll, "RegQueryValueExA");
        }
    }
    return &RegAPI;
}

static char const* msvc_find_sdk(char** latest_version, Allocator* allocator)
{
    HKEY h_key;
    const char* root_sub_key = "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
    char root_path[512];
    DWORD root_path_size = sizeof(root_path);
    DWORD index = 0;
    *latest_version = string_from_c_str(allocator, "");
    struct RegAPI* reg_api = msvc_get_reg_api();

    LONG result = reg_api->RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        root_sub_key,
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &h_key);
    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    while (1)
    {
        char version[32];
        DWORD name_length = sizeof(version);
        result = reg_api->RegEnumKeyExA(
            h_key,
            index,
            version,
            &name_length,
            NULL,
            NULL,
            NULL,
            NULL);

        if (result == ERROR_NO_MORE_ITEMS)
        {
            break;
        }
        else if (result != ERROR_SUCCESS)
        {
            reg_api->RegCloseKey(h_key);
            return NULL;
        }

        if (strchr(version, '.') != NULL &&
            strcmp(version, *latest_version) > 0)
        {
            array_resize(allocator, *latest_version, 0);
            string_concat_c_str(allocator, *latest_version, version);
        }
        index++;
    }
    reg_api->RegCloseKey(h_key);

    if (array_size(*latest_version) == 0)
    {
        return NULL;
    }

    result = reg_api->RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        root_sub_key,
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &h_key);
    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    result = reg_api->RegQueryValueExA(
        h_key,
        "KitsRoot10",
        NULL,
        NULL,
        (LPBYTE)root_path,
        &root_path_size);
    reg_api->RegCloseKey(h_key);

    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    return string_from_c_str(allocator, root_path);
}

static char* msvc_find_installation_path(Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* pf86 = os_get_env(temp_allocator, "ProgramFiles(x86)");
    if (pf86 == NULL)
    {
        error("os_get_env: cannot get env: ProgramFiles(x86)");
        return NULL;
    }
    char const* vswhere_path = path_combine(temp_allocator, pf86, "Microsoft Visual Studio\\Installer\\vswhere.exe", NULL);
    if (!os_file_exists(vswhere_path))
    {
        error("vswhere.exe not found!");
        return NULL;
    }
    char const* cmd = string_from_print(temp_allocator, "\"%s\" -products * -latest -property installationPath", vswhere_path);
    FILE* f = os_popen(cmd, "rb");
    char* result = NULL;
    if (f)
    {
        while (true)
        {
            int ch = getc(f);
            if (ch == EOF)
            {
                break;
            }
            if (ch != '\r')
            {
                array_push(allocator, result, (char)ch);
            }
            else
            {
                array_push(allocator, result, 0);
                array_pop(result);
                break;
            }
        }
        os_pclose(f);
    }
    if (array_size(result) == 0)
    {
        error("msvc not found!");
        return NULL;
    }
    allocator_destroy(temp_allocator);
    return result;
}

static char const* msvc_find_latest_version(char const* root_path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* msvc_dir = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC", root_path);
    Directory* d = directory_open(msvc_dir, temp_allocator);
    if (d == NULL)
    {
        allocator_destroy(temp_allocator);
        return NULL;
    }
    char version[32] = "";
    while (true)
    {
        DirectoryEntry* entry = directory_read(d);
        if (entry == NULL)
        {
            directory_close(d);
            allocator_destroy(temp_allocator);
            break;
        }
        if (string_equal(entry->name, ".") || string_equal(entry->name, ".."))
        {
            continue;
        }
        if (entry->is_directory)
        {
            if (strchr(entry->name, '.') != NULL &&
                strcmp(entry->name, version) > 0)
            {
                strcpy(version, entry->name);
            }
        }
    }
    if (version[0] == 0)
    {
        return NULL;
    }
    return string_from_c_str(allocator, version);
}

static bool msvc_get_env_strings(
    Allocator* allocator,
    ArchitectureType type,
    char** out_include,
    char** out_lib,
    char** out_path)
{
    char const* arch = NULL;
    if (type == ARCH_X86)
    {
        arch = "x86";
    }
    else if (type == ARCH_X64)
    {
        arch = "x64";
    }
    else
    {
        error("Unsupported architecture!");
        exit(EXIT_FAILURE);
    }

    Allocator* temp_allocator = allocator_temp();
    char* sdk_version = NULL;
    char const* sdk_path = msvc_find_sdk(&sdk_version, temp_allocator);
    char const* vs_path = msvc_find_installation_path(temp_allocator);
    if (vs_path == NULL)
    {
        return false;
    }
    char const* msvc_path = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC\\", vs_path);
    char const* msvc_version = msvc_find_latest_version(vs_path, temp_allocator);
    char* new_include = NULL;
    {
        string_printf(allocator, new_include, "%sInclude\\%s\\ucrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\um;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\shared;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\winrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\cppwinrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%s%s\\include;", msvc_path, msvc_version);
    }
    char* new_lib = NULL;
    {
        string_printf(allocator, new_lib, "%sLib\\%s\\um\\%s;", sdk_path, sdk_version, arch);
        string_printf(allocator, new_lib, "%sLib\\%s\\ucrt\\%s;", sdk_path, sdk_version, arch);
        string_printf(allocator, new_lib, "%s%s\\lib\\%s;", msvc_path, msvc_version, arch);
    }
    char* new_path = NULL;
    {
        string_printf(allocator, new_path, "%s%s\\bin\\Host%s\\%s;", msvc_path, msvc_version, arch, arch);
        string_printf(allocator, new_path, "%sbin\\%s\\%s;", sdk_path, sdk_version, arch);
    }
    *out_include = new_include;
    *out_lib = new_lib;
    *out_path = new_path;
    return true;
}

static int msvc_gen_env_file(Node* cmd)
{
    ArchitectureType type = (ArchitectureType)(uintptr_t)cmd->ctx;
    Allocator* allocator = allocator_create_chained();
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    FILE* out_file = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto Error;
    }
    char const* strings[] = {
        new_include,
        new_lib,
        new_path,
    };
    out_file = os_fopen(cmd->outputs[0]->path, "wb");
    if (!out_file)
    {
        goto Error;
    }
    size_t n = fwrite(&type, sizeof(ArchitectureType), 1, out_file);
    if (n != 1)
    {
        goto Error;
    }
    for (size_t i = 0; i != static_array_size(strings); i++)
    {
        char const* p = strings[i];
        uint32_t num_bytes = array_bytes(p);
        n = fwrite(&num_bytes, sizeof(uint32_t), 1, out_file);
        if (n != 1)
        {
            goto Error;
        }
        n = fwrite(p, 1, num_bytes, out_file);
        if (n != num_bytes)
        {
            goto Error;
        }
    }
    fclose(out_file);
    out_file = NULL;
    int exit_code = EXIT_SUCCESS;
    goto Found;
Error:
    if (out_file)
    {
        fclose(out_file);
    }
    exit_code = EXIT_FAILURE;
Found:
    allocator_destroy(allocator);
    return exit_code;
}

static bool msvc_gen_env_file_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    ArchitectureType type = (ArchitectureType)(uintptr_t)cmd->ctx;
    Allocator* allocator = allocator_create_chained();
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    FILE* file = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto Dirty;
    }
    bool b_dirty = false;
    Node* output = cmd->outputs[0];
    file = os_fopen(output->path, "rb");
    ArchitectureType old_type;
    size_t n = fread(&old_type, sizeof(ArchitectureType), 1, file);
    if (n != 1 || old_type != type)
    {
        goto Dirty;
    }
    char const* strings[] = {
        new_include,
        new_lib,
        new_path,
    };
    for (size_t i = 0; i != static_array_size(strings); i++)
    {
        char const* p = strings[i];
        uint32_t num_bytes = 0;
        n = fread(&num_bytes, sizeof(uint32_t), 1, file);
        if (n != 1 || num_bytes != array_bytes(p))
        {
            goto Dirty;
        }
        char* old_str = allocator_malloc(allocator, num_bytes);
        n = fread(old_str, 1, num_bytes, file);
        if (n != num_bytes || memcmp(p, old_str, num_bytes) != 0)
        {
            goto Dirty;
        }
    }
    goto NotDirty;
Dirty:
    b_dirty = true;
NotDirty:
    if (file)
    {
        fclose(file);
    }
    allocator_destroy(allocator);
    return b_dirty;
}

static wchar_t* envs[ARCH_COUNT];

bool msvc_set_arch_env(ArchitectureType type)
{
    if (type == ARCH_UNSPECIFIED)
    {
        return true;
    }
    Allocator* allocator = allocator_create_tiny(512, 4096);
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto NotFound;
    }

    char const* old_path = os_get_env(allocator, "Path");
    new_path = string_from_print(allocator, "%s;%s", new_path, old_path);
    os_set_env("INCLUDE", new_include);
    os_set_env("LIB", new_lib);
    os_set_env("Path", new_path);
    bool ret = true;
    goto Found;
NotFound:
    ret = false;
Found:
    assert(allocator);
    allocator_destroy(allocator);
    return ret;
}

static void msvc_make_env_block(ArchitectureType arch, wchar_t** out)
{
    wchar_t* default_env = os_get_default_env();
    if (msvc_set_arch_env(arch))
    {
        *out = os_get_env_block(allocator_c());
        SetEnvironmentStringsW(default_env);
    }
    else
    {
        *out = NULL;
    }
}

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    if (toolchain_type != TOOLCHAIN_TYPE_MSVC)
    {
        return NULL;
    }
    char const* path = NULL;
    if (arch == ARCH_X64)
    {
        path = "{out_dir}/envs/msvc_x64";
    }
    else if (arch == ARCH_X86)
    {
        path = "{out_dir}/envs/msvc_x86";
    }
    else
    {
        error("Not supported!");
        exit(EXIT_FAILURE);
    }
    Node* node = FILE(path);
    if (node->build_cmd == NULL)
    {
        Node* cmd = CALLBACK_CMD(msvc_gen_env_file, (void*)arch);
        cmd_add_output(cmd, node);
        node_set_check_dirty_fn(cmd, msvc_gen_env_file_check_dirty);
        if (envs[arch] == NULL)
        {
            msvc_make_env_block(arch, &envs[arch]);
        }
        node->env_block = envs[arch];
    }
    return node;
}

Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return msvc_get_env_node(toolchain_type, arch);
}

char const* msvc_find_std_module_source(bool b_compat)
{
    char const* filename = "std.ixx";
    if (b_compat)
    {
        filename = "std.compat.ixx";
    }

    Allocator* temp_allocator = allocator_temp();
    char const* vs_dir = msvc_find_installation_path(temp_allocator);
    if (!vs_dir)
    {
        return NULL;
    }
    char const* msvc_path = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC\\", vs_dir);
    char const* msvc_version = msvc_find_latest_version(vs_dir, temp_allocator);
    if (!msvc_version)
    {
        return NULL;
    }
    char const* module_path = string_from_print(temp_allocator, "%s%s\\modules\\%s", msvc_path, msvc_version, filename);
    return module_path;
}

ToolchainType c_toolchain_select_toolchain_automatically()
{
    ToolchainType toolchain = get_toolchain_by_current_compiler();
    Allocator* temp_allocator = allocator_temp();
    bool b_no_msvc = false;
    bool b_no_llvm = false;
    bool b_no_gcc = false;

    char const* clang_exe = get_clang_c_compiler();
    char const* clang_cmd = fmt("{} --version > NUL 2>&1", clang_exe);

    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        if (msvc_find_installation_path(temp_allocator))
        {
            return TOOLCHAIN_TYPE_MSVC;
        }
        else
        {
            b_no_msvc = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (system(clang_cmd) == 0)
        {
            return TOOLCHAIN_TYPE_LLVM;
        }
        else
        {
            b_no_llvm = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        if (system("gcc -v > NUL 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_GCC;
        }
        else
        {
            b_no_gcc = true;
        }
    }
    if (!b_no_msvc && msvc_find_installation_path(temp_allocator))
    {
        return TOOLCHAIN_TYPE_MSVC;
    }
    if (!b_no_llvm && system(clang_cmd) == 0)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (!b_no_gcc && system("gcc -v > NUL 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    error("Cannot find the C compiler");
    exit(EXIT_FAILURE);
}
