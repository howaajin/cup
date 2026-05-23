#pragma once

#include "core/platform.h"

#if CURRENT_PLATFORM == PLATFORM_MACOS

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/event.h> // kqueue 头文件
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
    KQ_CTX_THREAD,
    KQ_CTX_STDOUT = KQ_CTX_THREAD,
    KQ_CTX_STDERR,
} KqueueContextType;

struct ExecutorSlot
{
    union
    {
        KqueueContextType ctx_thread;
        KqueueContextType ctx_stdout;
    };
    KqueueContextType ctx_stderr;
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
            int thread_done_pipe[2]; // [0] read, [1] write (替代 eventfd)
        };
    };
    Task* task;
    int kq_fd; // kqueue file descriptor
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    int kq_fd;
};

#endif