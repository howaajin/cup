#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "core/os.h"

#include "core/codecvt.h"
#include "core/path.h"
#include "core/string.h"

#include <bcrypt.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <userenv.h>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Userenv.lib")

uint64_t os_get_mtime(char const* path)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(temp_allocator, path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &data))
    {
        FILETIME ft_modify = data.ftLastWriteTime;
        return (uint64_t)ft_modify.dwLowDateTime | (uint64_t)ft_modify.dwHighDateTime << 32;
    }
    return 0;
}

bool os_file_exists(char const* path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

char* os_get_env(Allocator* allocator, const char* name)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wname = utf8_to_wchars(temp_allocator, name);
    wchar_t value[32768];
    if (!GetEnvironmentVariableW(wname, value, sizeof(value)))
    {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        {
            return NULL;
        }
    }
    return wchars_to_utf8(allocator, value);
}

void os_set_env(char const* name, char const* env)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wname = utf8_to_wchars(temp_allocator, name);
    wchar_t* w_env = env ? utf8_to_wchars(temp_allocator, env) : NULL;
    if (SetEnvironmentVariableW(wname, w_env) == 0)
    {
        fprintf(stderr, "Failed to set environment variable: %s\n", name);
    }
}

wchar_t* os_get_env_block(Allocator* allocator)
{
    wchar_t* env = GetEnvironmentStringsW();
    wchar_t* copy = NULL;
    for (wchar_t* p = env; *p != 0 || *(p + 1) != 0; p++)
    {
        array_push(allocator, copy, *p);
    }
    FreeEnvironmentStringsW(env);
    array_push(allocator, copy, L'\0');
    array_push(allocator, copy, L'\0');
    return copy;
}

wchar_t* os_get_default_env(void)
{
    static wchar_t* default_env = NULL;

    if (!default_env)
    {
        CreateEnvironmentBlock((LPVOID*)&default_env, NULL, FALSE);
        SetEnvironmentStringsW(default_env);
    }
    return default_env;
}

void os_reset_env(void)
{
    wchar_t* default_env = os_get_default_env();
    SetEnvironmentStringsW(default_env);
}

#define MKDIR(path) _mkdir(path)

char* os_get_cwd(Allocator* allocator)
{
    char* p = _getcwd(NULL, 0);
    char* cwd = string_from_c_str(allocator, p);
    free(p);
    return cwd;
}

bool os_set_cwd(char const* path)
{
    return _chdir(path) == 0;
}

char const* os_get_cmdline(void)
{
    static char* utf8_cmdline = NULL;
    if (!utf8_cmdline)
    {
        wchar_t* wcmd = GetCommandLineW();
        utf8_cmdline = wchars_to_utf8(allocator_c(), wcmd);
    }
    return utf8_cmdline;
}

bool os_remove_file(char const* path)
{
    Allocator* allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(allocator, path);
    bool b = DeleteFileW(wpath);
    return b;
}

char* os_get_current_exe_path(Allocator* allocator)
{
    wchar_t temp[4096];
    GetModuleFileNameW(NULL, temp, 4096);
    char* path = wchars_to_utf8(allocator, temp);
    path_backslash_to_slash(path);
    return path;
}

bool os_rename(char const* old_path, char const* new_path)
{
    Allocator* allocator = allocator_temp();
    wchar_t* src_w = utf8_to_wchars(allocator, old_path);
    wchar_t* dst_w = utf8_to_wchars(allocator, new_path);
    bool success = MoveFileExW(src_w, dst_w, MOVEFILE_REPLACE_EXISTING);
    return success;
}

bool os_copy_file(char const* src, char const* dst)
{
    Allocator* allocator = allocator_temp();
    wchar_t* src_w = utf8_to_wchars(allocator, src);
    wchar_t* dst_w = utf8_to_wchars(allocator, dst);
    bool success = CopyFileW(src_w, dst_w, false);
    return success;
}

bool os_mkdir(char const* path)
{
    return _mkdir(path) == 0;
}

uint64_t os_get_rand_uint64()
{
    uint64_t result;
    BCryptGenRandom(NULL, (PBYTE)&result, sizeof(uint64_t), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return result;
}

int os_get_cpu_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

FILE* os_fopen(char const* path, char const* mode)
{
    Allocator* allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(allocator, path);
    wchar_t* wmode = utf8_to_wchars(allocator, mode);
    return _wfopen(wpath, wmode);
}

FILE* os_popen(char const* cmd, char const* mode)
{
    return _popen(cmd, mode);
}

int os_pclose(FILE* file)
{
    return _pclose(file);
}

Process* os_start_process(char const* cmd)
{
    Allocator* temp_allocator = allocator_create_chained();
    STARTUPINFOW si = {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {0};
    wchar_t* cwd = NULL;
    wchar_t* cmd_w = utf8_to_wchars(temp_allocator, cmd);
    if (!CreateProcessW(NULL, cmd_w, NULL, NULL, FALSE, 0, NULL, cwd, &si, &pi))
    {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND)
        {
            fprintf(stderr, "error: os_start_process: CreateProcess failed");
        }
    }
    else
    {
        CloseHandle(pi.hThread);
    }
    allocator_destroy(temp_allocator);
    return pi.hProcess;
}

int os_wait_process(Process* p)
{
    WaitForSingleObject(p, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(p, &exit_code);
    return (int)exit_code;
}

void os_forget_process(Process* p)
{
    CloseHandle(p);
}

uint64_t os_get_file_size(char const* path)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(temp_allocator, path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &data))
    {
        return (uint64_t)data.nFileSizeLow | (uint64_t)data.nFileSizeHigh << 32;
    }
    return UINT64_MAX;
}

char* os_full_path(char const* path, Allocator* allocator)
{
    wchar_t* wpath = utf8_to_wchars(allocator_temp(), path);
    wchar_t buffer[4096];
    GetFullPathNameW(wpath, 4096, buffer, NULL);
    char* result = wchars_to_utf8(allocator, buffer);
    path_backslash_to_slash(result);
    return result;
}

typedef struct LockFileContextWindows
{
    FILE* file;
    HANDLE handle;
    Allocator* allocator;
    OVERLAPPED overlapped;
} LockFileContextWindows;

LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared)
{
    os_ensure_dir_existed(path);
    HANDLE handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }
    LockFileContextWindows* ctx = allocator_calloc(allocator, 1, sizeof(LockFileContextWindows));
    ctx->handle = handle;
    DWORD lock_flags = b_shared ? 0 : LOCKFILE_EXCLUSIVE_LOCK;
    BOOL lock_result = LockFileEx(
        handle,
        lock_flags,
        0,
        MAXDWORD,
        MAXDWORD,
        &ctx->overlapped);
    if (!lock_result)
    {
        allocator_free(allocator, ctx);
        CloseHandle(handle);
        return NULL;
    }
    int fd = _open_osfhandle((intptr_t)handle, _O_RDWR);
    ctx->allocator = allocator;
    ctx->file = _fdopen(fd, "w+");
    return (LockFileContext*)ctx;
}

bool os_unlock_file(LockFileContext* context)
{
    LockFileContextWindows* ctx = (LockFileContextWindows*)context;
    if (context == NULL)
    {
        return false;
    }
    fflush(ctx->file);
    bool succeeded = UnlockFileEx(
        ctx->handle,
        0,
        MAXDWORD,
        MAXDWORD,
        &ctx->overlapped);
    fclose(ctx->file);
    allocator_free(ctx->allocator, context);
    return succeeded;
}

int os_ftruncate(FILE* f, long size)
{
    return _chsize(_fileno(f), size);
}

bool os_file_writable(const char* path)
{
    if (!os_file_exists(path))
    {
        return true;
    }
    FILE* try_file = os_fopen(path, "a");
    if (try_file == NULL)
    {
        return false;
    }
    else
    {
        fclose(try_file);
    }

    HANDLE h = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    CloseHandle(h);
    return true;
}

bool os_is_terminal_supports_color(void)
{
#define isatty _isatty
#define fileno _fileno

    static bool b_terminal_supports_color = false;
    static bool b_terminal_supports_color_checked = false;
    if (!b_terminal_supports_color_checked)
    {
        b_terminal_supports_color_checked = true;
        if (!isatty(fileno(stderr)))
        {
            b_terminal_supports_color = false;
            return false;
        }

        HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE)
        {
            b_terminal_supports_color = false;
            return false;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode))
        {
            b_terminal_supports_color = false;
            return false;
        }
        b_terminal_supports_color = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    }
    return b_terminal_supports_color;
#undef isatty
#undef fileno
}

void os_set_console_utf8(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

int os_mtx_init(Mutex* mtx)
{
    *mtx = allocator_calloc(allocator_c(), 1, sizeof(CRITICAL_SECTION));
    CRITICAL_SECTION* ritical_section = *mtx;
    InitializeCriticalSection(ritical_section);
    return 0;
}

void os_mtx_destroy(Mutex* mtx)
{
    DeleteCriticalSection(*mtx);
    allocator_free(allocator_c(), *mtx);
    *mtx = NULL;
}

int os_mtx_lock(Mutex* mtx)
{
    EnterCriticalSection(*mtx);
    return 0;
}

int os_mtx_unlock(Mutex* mtx)
{
    LeaveCriticalSection(*mtx);
    return 0;
}

_Static_assert(sizeof(os_tss_t) == sizeof(DWORD), "os_tss_t size mismatch");
_Static_assert(sizeof(os_once_flag) == sizeof(INIT_ONCE), "os_once_flag size mismatch");

static BOOL CALLBACK windows_once_callback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
    void (*func)(void) = (void (*)(void))Parameter;
    func();
    return TRUE;
}

void os_call_once(os_once_flag* flag, void (*func)(void))
{
    InitOnceExecuteOnce((PINIT_ONCE)flag, windows_once_callback, (void*)func, NULL);
}

int os_tss_create(os_tss_t* key, os_tss_dtor_t dtor)
{
    *key = (os_tss_t)FlsAlloc((PFLS_CALLBACK_FUNCTION)dtor);
    return (*key == FLS_OUT_OF_INDEXES) ? 0 : 1;
}

void* os_tss_get(os_tss_t key)
{
    return FlsGetValue((DWORD)key);
}

int os_tss_set(os_tss_t key, void* val)
{
    return FlsSetValue((DWORD)key, val) ? 1 : 0;
}

void os_tss_delete(os_tss_t key)
{
    FlsFree((DWORD)key);
}
