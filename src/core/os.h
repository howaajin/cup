#pragma once

#include "core/platform.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Allocator Allocator;
typedef struct Process Process;
typedef struct LockFileContext
{
    FILE* file;
} LockFileContext;

uint64_t os_get_mtime(char const* path);
bool os_file_exists(char const* path);
char* os_get_env(Allocator* allocator, const char* name);
void os_set_env(char const* name, char const* env);
wchar_t* os_get_env_block(Allocator* allocator);
wchar_t* os_get_default_env(void);
void os_reset_env(void);
char* os_get_cwd(Allocator* allocator);
bool os_set_cwd(char const* path);
void os_ensure_dir_existed(char const* path);
char const* os_get_cmdline(void);
bool os_remove_file(char const* path);
char* os_get_current_exe_path(Allocator* allocator);
bool os_rename(char const* old_path, char const* new_path);
bool os_copy_file(char const* src, char const* dst);
bool os_mkdir(char const* path);
bool os_create_directory_tree(char const* path);
char* os_create_guid(Allocator* allocator, bool lowercase);
uint64_t os_get_rand_uint64(void);
int os_get_cpu_count(void);
FILE* os_fopen(char const* path, char const* mode);
FILE* os_popen(char const* cmd, char const* mode);
int os_pclose(FILE* file);
Process* os_start_process(char const* cmd);
int os_wait_process(Process* p);
void os_forget_process(Process* p);
uint64_t os_get_file_size(char const* path);
char* os_read_all(Allocator* allocator, char const* path);
bool os_write_all(char const* path, char const* content, size_t size);
char* os_full_path(char const* path, Allocator* allocator);
LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared);
bool os_unlock_file(LockFileContext* context);
int os_ftruncate(FILE* f, long size);
bool os_file_writable(const char* path);
bool os_is_terminal_supports_color(void);
void os_set_console_utf8(void);

typedef void* Mutex;
int os_mtx_init(Mutex* mtx);
void os_mtx_destroy(Mutex* mtx);
int os_mtx_lock(Mutex* mtx);
int os_mtx_unlock(Mutex* mtx);

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define OS_API_CALLCONV __stdcall
typedef unsigned long os_tss_t;
typedef struct
{
    void* Ptr;
} os_once_flag;
#define OS_ONCE_FLAG_INIT {0}
#else
#include <pthread.h>
#define OS_API_CALLCONV
typedef pthread_key_t os_tss_t;
typedef pthread_once_t os_once_flag;
#define OS_ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#endif

typedef void(OS_API_CALLCONV* os_tss_dtor_t)(void*);

int os_tss_create(os_tss_t* key, os_tss_dtor_t dtor);
void* os_tss_get(os_tss_t key);
int os_tss_set(os_tss_t key, void* val);
void os_tss_delete(os_tss_t key);
void os_call_once(os_once_flag* flag, void (*func)(void));
