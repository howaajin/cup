#pragma once

#include "core/platform.h"

#if CURRENT_PLATFORM == PLATFORM_LINUX

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define OUTPUT_BUFFER_SIZE 1024

typedef struct Task Task;
typedef struct ReadPipeContext ReadPipeContext;
typedef struct ExecutorSlot ExecutorSlot;
typedef struct Executor Executor;
typedef struct Allocator Allocator;

struct ReadPipeContext
{
    int read_pipe;
    void (*write_buffer)(void* ctx, char const* buffer, size_t num_bytes);
    void* write_buffer_ctx;
};

typedef enum
{
    EPOLL_CTX_THREAD,
    EPOLL_CTX_STDOUT = EPOLL_CTX_THREAD,
    EPOLL_CTX_STDERR,
} EpollContextType;

struct ExecutorSlot
{
    union
    {
        EpollContextType ctx_thread;
        EpollContextType ctx_stdout;
    };
    EpollContextType ctx_stderr;
    union
    {
        struct
        {
            pid_t pid;
            ReadPipeContext read_stdout_ctx;
            ReadPipeContext read_stderr_ctx;
        };
        struct
        {
            pthread_t thread_id;
            int event_fd;
        };
    };
    Task* task;
    int epoll_fd;
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    int epoll_fd;
};

#endif