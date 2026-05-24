#include "core/os.h"
#include "core/macros.h"
#include "core/path.h"
#include "core/string.h"
#include "core/utilities.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

uint64_t os_get_mtime(char const* path)
{
    struct stat file_stat;

    if (stat(path, &file_stat) == -1)
    {
        return 0;
    }
    return file_stat.st_mtime;
}

bool os_file_exists(char const* path)
{
    return access(path, F_OK) == 0;
}

char* os_get_cwd(Allocator* allocator)
{
    char* p = getcwd(NULL, 0);
    char* cwd = string_from_c_str(allocator, p);
    free(p);
    return cwd;
}

bool os_set_cwd(char const* path)
{
    return chdir(path) == 0;
}

bool os_mkdir(char const* path)
{
    if (mkdir(path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            return false;
        }
    }
    return true;
}

bool os_remove_file(char const* path)
{
    return remove(path) == 0;
}

bool os_rename(char const* old_path, char const* new_path)
{
    return rename(old_path, new_path) == 0;
}

bool os_copy_file(char const* src, char const* dst)
{
    if (fork() == 0)
    {
        execlp("cp", "cp", src, dst, NULL);
        exit(0);
    }
    wait(NULL);
    return true;
}

char* get_absolute_path(char const* path, Allocator* allocator)
{
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char* cwd = os_get_cwd(stack_allocator);
    if (!path_is_absolute(path))
    {
        return path_combine(allocator, cwd, path, NULL);
    }
    return string_from_c_str(allocator, path);
}

uint64_t os_get_file_size(char const* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return st.st_size;
    }
    return UINT64_MAX;
}

char* os_get_env(Allocator* allocator, const char* name)
{
    char* e = getenv(name);
    return string_from_c_str(allocator, e);
}

void os_set_env(char const* name, char const* env)
{
    setenv(name, env, 1);
}

void os_reset_env(void)
{
}

FILE* os_fopen(char const* path, char const* mode)
{
    return fopen(path, mode);
}

FILE* os_popen(char const* cmd, char const* mode)
{
    return popen(cmd, mode);
}

int os_pclose(FILE* file)
{
    return pclose(file);
}

int os_get_cpu_count(void)
{
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count == -1)
    {
        perror("sysconf failed");
        return 1;
    }
    return cpu_count;
}

Process* os_start_process(char const* cmd)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        Allocator* stack_allocator = allocator_arena_from_alloca(65535);
        char* cmd_copied = string_from_c_str(stack_allocator, cmd);
        char const* p = cmd_copied;
        char** argv = NULL;
        while (true)
        {
            char* arg = NULL;
            p = utilities_split_cmd(stack_allocator, p, &arg);
            if (array_size(arg) == 0)
            {
                array_push(stack_allocator, argv, NULL);
                break;
            }
            array_push(stack_allocator, argv, arg);
        }
        execvp(argv[0], argv);
        error("execv failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        return (Process*)(uintptr_t)pid;
    }
}

int os_wait_process(Process* p)
{
    pid_t pid = (uintptr_t)p;
    int status;
    pid_t wait_pid = waitpid(pid, &status, 0);
    if (wait_pid == -1)
    {
        error("waitpid failed");
        exit(EXIT_FAILURE);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

void os_forget_process(Process* process)
{
}

typedef struct LockFileContextLinux
{
    FILE* file;
    int fd;
    Allocator* allocator;
} LockFileContextLinux;

LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared)
{
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        perror("open failed");
        return NULL;
    }

    if (flock(fd, b_shared ? LOCK_SH : LOCK_EX) == -1)
    {
        perror("flock failed");
        close(fd);
        return NULL;
    }

    LockFileContextLinux* ctx = allocator_calloc(allocator, 1, sizeof(LockFileContextLinux));
    ctx->fd = fd;
    ctx->allocator = allocator;
    ctx->file = fdopen(fd, "w+");
    if (!ctx->file)
    {
        perror("fdopen failed");
        flock(fd, LOCK_UN);
        close(fd);
        allocator_free(allocator, ctx);
        return NULL;
    }

    return (LockFileContext*)ctx;
}

bool os_unlock_file(LockFileContext* context)
{
    LockFileContextLinux* ctx = (LockFileContextLinux*)context;
    if (!ctx)
    {
        return false;
    }

    fflush(ctx->file);
    bool succeeded = (flock(ctx->fd, LOCK_UN) == 0);
    fclose(ctx->file);
    allocator_free(ctx->allocator, context);
    return succeeded;
}

int os_ftruncate(FILE* f, long size)
{
    return ftruncate(fileno(f), size);
}

bool os_file_writable(char const* path)
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
        return true;
    }
}

bool os_is_terminal_supports_color(void)
{
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
        const char* term = getenv("TERM");
        b_terminal_supports_color = term != NULL && strcmp(term, "dumb") != 0;
    }
    return b_terminal_supports_color;
}

void os_set_console_utf8(void)
{
}

int os_mtx_init(Mutex* mtx)
{
    *mtx = allocator_calloc(allocator_c(), 1, sizeof(pthread_mutex_t));
    pthread_mutex_t* pthread_mutex = *mtx;
    if (pthread_mutex_init(pthread_mutex, NULL) == 0)
    {
        return 0;
    }
    return 1;
}

void os_mtx_destroy(Mutex* mtx)
{
    pthread_mutex_destroy(*mtx);
    allocator_free(allocator_c(), *mtx);
    *mtx = NULL;
}

int os_mtx_lock(Mutex* mtx)
{
    if (pthread_mutex_lock(*mtx) == 0)
    {
        return 0;
    }
    return 1;
}

int os_mtx_unlock(Mutex* mtx)
{
    if (pthread_mutex_unlock(*mtx) == 0)
    {
        return 0;
    }
    return 1;
}

int os_tss_create(os_tss_t* key, os_tss_dtor_t dtor)
{
    return (pthread_key_create(key, dtor) == 0) ? 1 : 0;
}

void* os_tss_get(os_tss_t key)
{
    return pthread_getspecific(key);
}

int os_tss_set(os_tss_t key, void* val)
{
    return (pthread_setspecific(key, val) == 0) ? 1 : 0;
}

void os_tss_delete(os_tss_t key)
{
    pthread_key_delete(key);
}

void os_call_once(os_once_flag* flag, void (*func)(void))
{
    pthread_once(flag, func);
}
