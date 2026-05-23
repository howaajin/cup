#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Allocator Allocator;
typedef struct Task Task;
typedef struct Executor Executor;
typedef struct ExecutorSlot ExecutorSlot;

struct Task
{
    union
    {
        struct
        {
            char* cmdline;
            void (*write_stdout)(void* ctx, char const* buffer, size_t num_bytes);
            void (*write_stderr)(void* ctx, char const* buffer, size_t num_bytes);
            wchar_t* env_block;
        };
        struct
        {
            int (*thread_fn)(Task* task, void* ctx);
        };
    };
    void* ctx;
    int exit_code;
    bool b_thread;
};

Executor* executor_create(Allocator* allocator, size_t num_slots);
void executor_destroy(Executor* executor);
Task* executor_create_process_task(Executor* executor, char const* cmdline);
Task* executor_create_thread_task(Executor* executor, int (*fn)(Task*, void* ctx), void* ctx);
void executor_destroy_task(Executor* executor, Task* task);
void executor_set_task_write_stdout_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes));
void executor_set_task_write_stderr_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes));
void executor_set_task_context(Task* task, void* ctx);
void* executor_get_task_context(Task* task);
int executor_get_task_exit_code(Task* task);
void executor_add_task(Executor* executor, Task* task);
Task* executor_update(Executor* executor);
Task* executor_wait(Executor* executor);
bool executor_is_full(Executor* executor);
bool executor_is_empty(Executor* executor);

// Windows only
void executor_set_task_env_block(Task* task, wchar_t* env_block);
void executor_set_default_env(Executor* executor, wchar_t* env_block);

// private
void executor_execute_slot_thread(ExecutorSlot* slot);
void executor_execute_slot_process(ExecutorSlot* slot);
void executor_set_slot_task(Executor* executor, uint32_t slot_id, Task* task);
void executor_force_kill_task(ExecutorSlot* slot);
void executor_flush(Executor* executor);
uint32_t executor_find_empty_slot(Executor* executor);
uint32_t executor_find_finished_slot(Executor* executor);
uint32_t executor_find_task_slot_id(Executor* executor, Task* task);
ExecutorSlot* executor_get_slot(Executor* executor, uint32_t slot_id);