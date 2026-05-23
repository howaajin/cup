#include "cup/executor/executor.h"
#include "core/array.h"
#include "core/platform.h"
#include "core/string.h"

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#include "cup/executor/executor_windows.h"
#elif CURRENT_PLATFORM == PLATFORM_LINUX
#include "cup/executor/executor_linux.h"
#elif CURRENT_PLATFORM == PLATFORM_MACOS
#include "cup/executor/executor_mac.h"
#else
#error "unknown platform"
#endif

#include <assert.h>

void executor_platform_init(Executor* executor);
void executor_platform_destroy(Executor* executor);
void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task);

Executor* executor_create(Allocator* allocator, size_t num_slots)
{
    Executor* executor = allocator_calloc(allocator, 1, sizeof(Executor));
    executor->allocator = allocator;
    executor->slots = allocator_calloc(allocator, num_slots, sizeof(ExecutorSlot));
    executor->num_slots = num_slots;
    executor->num_running_tasks = 0;
    executor->pending_tasks = NULL;
    executor_platform_init(executor);
    return executor;
}

void executor_destroy(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task)
        {
            executor_force_kill_task(slot);
        }
    }
    executor_platform_destroy(executor);
    allocator_free(executor->allocator, executor->slots);
    allocator_free(executor->allocator, executor);
}

bool executor_is_full(Executor* executor)
{
    return executor->num_running_tasks == executor->num_slots;
}

bool executor_is_empty(Executor* executor)
{
    return executor->num_running_tasks == 0 &&
           array_size(executor->pending_tasks) == 0;
}

uint32_t executor_find_empty_slot(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task == NULL)
        {
            return i + 1;
        }
    }
    return 0;
}

uint32_t executor_find_task_slot_id(Executor* executor, Task* task)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        if (executor->slots[i].task == task)
        {
            return i + 1;
        }
    }
    return 0;
}

uint32_t executor_find_finished_slot(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task == NULL)
        {
            continue;
        }
        if (slot->b_finished)
        {
            return i + 1;
        }
    }
    return 0;
}

ExecutorSlot* executor_get_slot(Executor* executor, uint32_t slot_id)
{
    return &executor->slots[slot_id - 1];
}

Task* executor_wait(Executor* executor)
{
    if (executor_is_empty(executor))
    {
        return NULL;
    }
    for (;;)
    {
        Task* task = executor_update(executor);
        if (task)
        {
            return task;
        }
    }
}

static void executor_execute_slot(Executor* executor, uint32_t slot_id)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (slot->task->b_thread)
    {
        executor_execute_slot_thread(slot);
    }
    else
    {
        executor_execute_slot_process(slot);
    }
}

static void executor_flush_task(Executor* executor, Task* task)
{
    uint32_t slot_id = executor_find_empty_slot(executor);
    assert(slot_id);
    executor_set_slot_task(executor, slot_id, task);
    executor_execute_slot(executor, slot_id);
    executor->num_running_tasks += 1;
}

void executor_flush(Executor* executor)
{
    while (executor->num_running_tasks != executor->num_slots && array_size(executor->pending_tasks))
    {
        Task* task = executor->pending_tasks[0];
        array_remove_unordered(executor->pending_tasks, 0);
        executor_flush_task(executor, task);
    }
}

void executor_add_task(Executor* executor, Task* task)
{
    if (executor_is_full(executor))
    {
        array_push(executor->allocator, executor->pending_tasks, task);
        return;
    }
    executor_flush_task(executor, task);
}

Task* executor_create_thread_task(Executor* executor, int (*fn)(Task*, void*), void* ctx)
{
    Task* task = allocator_calloc(executor->allocator, 1, sizeof(Task));
    task->thread_fn = fn;
    task->ctx = ctx;
    task->b_thread = true;
    return task;
}

Task* executor_create_process_task(Executor* executor, char const* cmdline)
{
    Task* task = allocator_calloc(executor->allocator, 1, sizeof(Task));
    task->cmdline = string_from_c_str(executor->allocator, cmdline);
    task->b_thread = false;
    task->exit_code = EXIT_FAILURE;
    return task;
}

void executor_destroy_task(Executor* executor, Task* task)
{
    allocator_free(executor->allocator, task);
}

void executor_set_task_write_stdout_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes))
{
    task->write_stdout = fn;
}

void executor_set_task_write_stderr_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes))
{
    task->write_stderr = fn;
}

void executor_set_task_context(Task* task, void* ctx)
{
    task->ctx = ctx;
}

void* executor_get_task_context(Task* task)
{
    return task->ctx;
}

int executor_get_task_exit_code(Task* task)
{
    return task->exit_code;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes)
{
}

void executor_set_slot_task(Executor* executor, uint32_t slot_id, Task* task)
{
    executor_platform_set_slot(executor, slot_id, task);
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    slot->task = task;
    slot->b_finished = false;
    if (!task->b_thread)
    {
        if (task->write_stdout)
        {
            slot->read_stdout_ctx.write_buffer = task->write_stdout;
        }
        else
        {
            slot->read_stdout_ctx.write_buffer = executor_default_write_buffer;
        }
        if (task->write_stderr)
        {
            slot->read_stderr_ctx.write_buffer = task->write_stderr;
        }
        else
        {
            slot->read_stderr_ctx.write_buffer = slot->read_stdout_ctx.write_buffer;
        }
        slot->read_stdout_ctx.write_buffer_ctx = task->ctx;
        slot->read_stderr_ctx.write_buffer_ctx = task->ctx;
    }
}
