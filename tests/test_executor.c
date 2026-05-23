#include "core/allocator.h"
#include "core/os.h"
#include "core/platform.h"
#include "core/string.h"
#include "cup/test.h"

#include "cup/executor/executor.h"

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#include "cup/executor/executor_windows.h"
#elif CURRENT_PLATFORM == PLATFORM_LINUX
#include "cup/executor/executor_linux.h"
#elif CURRENT_PLATFORM == PLATFORM_MACOS
#include "cup/executor/executor_mac.h"
#endif

TEST(test_executor_create, executor)
{
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    ASSERT(executor);
    ASSERT(executor->allocator);
    ASSERT(executor->slots);
    ASSERT(executor->num_slots == 8);
    ASSERT(executor->num_running_tasks == 0);
    ASSERT(executor->pending_tasks == NULL);
}

static int test_thread_task_fn(Task* task, void* ctx)
{
    return 42;
}

TEST(test_executor_add_thread_task, executor)
{
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    Task* task = executor_create_thread_task(executor, test_thread_task_fn, NULL);
    ASSERT(task);
    executor_add_task(executor, task);
    ASSERT(executor->slots[0].task == task);
    executor_wait(executor);
    ASSERT(task->exit_code == 42);
    executor_destroy(executor);
    allocator_destroy(allocator);
}

static void test_executor_write_buffer(void* ctx, char const* buffer, size_t num_bytes)
{
    struct
    {
        Allocator* allocator;
        char* output;
    }* c = ctx;
    string_printf(c->allocator, c->output, "%.*s", (int)num_bytes, buffer);
}

TEST(test_executor_add_process_task, executor)
{
    char const* cmdline = NULL;
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        cmdline = "cmd /c echo hello";
    }
    else
    {
        cmdline = "echo hello";
    }
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    Task* task = executor_create_process_task(executor, cmdline);
    ASSERT(task);
    task->write_stdout = test_executor_write_buffer;
    struct
    {
        Allocator* allocator;
        char* output;
    } ctx = {allocator, NULL};
    task->ctx = &ctx;
    executor_add_task(executor, task);
    uint32_t slot_id = executor_find_task_slot_id(executor, task);
    ASSERT(slot_id);
    executor_wait(executor);
    ASSERT(task->exit_code == EXIT_SUCCESS);
    ASSERT(string_starts_with(ctx.output, "hello"));
    executor_destroy(executor);
    allocator_destroy(allocator);
}

TEST(test_nonexistent_command, executor)
{
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    Task* task = executor_create_process_task(executor, "nonexistent_command");
    executor_add_task(executor, task);
    executor_wait(executor);
    ASSERT(task->exit_code != EXIT_SUCCESS);
    executor_destroy(executor);
    allocator_destroy(allocator);
}

TEST(test_executor_max_jobs, executor)
{
    Allocator* allocator = allocator_create_chained();
    size_t max_jobs = 2;
    size_t num_jobs = 4;
    Executor* executor = executor_create(allocator, max_jobs);
    {
        for (size_t i = 0; i != num_jobs; i++)
        {
            Task* task = executor_create_process_task(executor, "nonexistent_command");
            executor_add_task(executor, task);
        }
    }
    ASSERT(executor->num_running_tasks == max_jobs);
    size_t num_finished = 0;
    while (true)
    {
        Task* task = executor_wait(executor);
        if (task)
        {
            num_finished += 1;
        }
        if (executor_is_empty(executor))
        {
            break;
        }
    }
    ASSERT(num_finished == num_jobs);
    executor_destroy(executor);
    allocator_destroy(allocator);
}

TEST(test_executor_process_stderr_output, executor)
{
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 2);
    char const* cmdline =
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
        "cmd /c echo error 1>&2";
#else
        "echo error >&2";
#endif
    struct
    {
        Allocator* allocator;
        char* output;
    } ctx = {allocator, NULL};
    Task* task = executor_create_process_task(executor, cmdline);
    task->write_stderr = test_executor_write_buffer;
    task->ctx = &ctx;
    executor_add_task(executor, task);
    executor_wait(executor);
    ASSERT(string_starts_with(ctx.output, "error"));
    executor_destroy(executor);
    allocator_destroy(allocator);
}

TEST(test_force_destroy, executor)
{
    char const* cmdline = NULL;
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        cmdline = "cmd /c exit 7";
    }
    else
    {
        cmdline = "sh -c \"exit 7\"";
    }
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    Task* task = executor_create_process_task(executor, cmdline);
    executor_add_task(executor, task);
    uint32_t slot_id = executor_find_task_slot_id(executor, task);
    ASSERT(slot_id);
    executor_destroy(executor);
    allocator_destroy(allocator);
}

TEST(test_executor_env_block, executor)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
#define TEST_ENV_NAME "TEST_ENV_VAR_71"
    Allocator* allocator = allocator_create_chained();
    Executor* executor = executor_create(allocator, 8);
    wchar_t* default_env_block = os_get_env_block(allocator);
    executor_set_default_env(executor, default_env_block);
    os_set_env(TEST_ENV_NAME, "test_env_value");
    wchar_t* test_env_block = os_get_env_block(allocator);
    os_set_env(TEST_ENV_NAME, NULL);
    Task* task = executor_create_process_task(executor, "cmd /c echo %" TEST_ENV_NAME "%");
    task->write_stdout = test_executor_write_buffer;
    struct
    {
        Allocator* allocator;
        char* output;
    } ctx = {allocator, NULL};
    task->ctx = &ctx;
    executor_set_task_env_block(task, test_env_block);
    executor_add_task(executor, task);
    task = executor_wait(executor);
    ASSERT(task);
    ASSERT(task->exit_code == EXIT_SUCCESS);
    ASSERT(string_starts_with(ctx.output, "test_env_value"));
    task = executor_create_process_task(executor, "cmd /c echo %" TEST_ENV_NAME "%");
    task->write_stdout = test_executor_write_buffer;
    ctx.output = NULL;
    task->ctx = &ctx;
    executor_add_task(executor, task);
    task = executor_wait(executor);
    ASSERT(task);
    ASSERT(task->exit_code == EXIT_SUCCESS);
    ASSERT(string_starts_with(ctx.output, "%" TEST_ENV_NAME "%"));
    executor_destroy(executor);
    allocator_destroy(allocator);
#undef TEST_ENV_NAME
}