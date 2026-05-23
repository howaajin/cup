#pragma once

#include "core/platform.h"

#if CURRENT_PLATFORM == PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdbool.h>

#define OUTPUT_BUFFER_SIZE 1024

typedef struct Task Task;
typedef struct ReadPipeContext ReadPipeContext;
typedef struct ExecutorSlot ExecutorSlot;
typedef struct Executor Executor;
typedef struct Allocator Allocator;

struct ReadPipeContext
{
    OVERLAPPED overlapped;
    HANDLE read_pipe_handle;
    HANDLE child_write_pipe_handle;
    char buffer[OUTPUT_BUFFER_SIZE];
    void (*write_buffer)(void* ctx, char const* buffer, size_t num_bytes);
    void* write_buffer_ctx;
};

struct ExecutorSlot
{
    union
    {
        struct
        {
            HANDLE process;
            ReadPipeContext read_stdout_ctx;
            ReadPipeContext read_stderr_ctx;
        };
        struct
        {
            HANDLE thread;
            OVERLAPPED overlapped;
        };
    };
    Task* task;
    Executor* executor;
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    HANDLE iocp;
    wchar_t* default_env_block;
    wchar_t* current_env_block;
};

#endif