#include "cup/executor/executor_windows.h"
#include "core/allocator.h"
#include "core/codecvt.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/string.h"
#include "cup/executor/executor.h"
#include "cup/fmt.h"

#include "core/macros.h"
#include <assert.h>

void executor_force_kill_task(ExecutorSlot* slot)
{
    if (slot->task->b_thread)
    {
        if (slot->thread)
        {
            TerminateThread(slot->thread, 1);
            CloseHandle(slot->process);
            slot->thread = NULL;
        }
    }
    else
    {
        if (slot->process)
        {
            TerminateProcess(slot->process, 1);
            CloseHandle(slot->process);
            slot->process = NULL;
        }
        if (slot->read_stdout_ctx.read_pipe_handle)
        {
            CloseHandle(slot->read_stdout_ctx.read_pipe_handle);
            slot->read_stdout_ctx.read_pipe_handle = NULL;
        }
        if (slot->read_stderr_ctx.read_pipe_handle)
        {
            CloseHandle(slot->read_stderr_ctx.read_pipe_handle);
            slot->read_stderr_ctx.read_pipe_handle = NULL;
        }
    }
}

void executor_platform_init(Executor* executor)
{
    executor->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    expect(executor->iocp != NULL, "failed to create IO completion port");
    wchar_t* env = os_get_default_env();
    executor_set_default_env(executor, env);
}

void executor_platform_destroy(Executor* executor)
{
    CloseHandle(executor->iocp);
}

static DWORD WINAPI executor_thread_wrapper(void* context)
{
    ExecutorSlot* slot = context;
    Task* task = slot->task;
    DWORD exit_code = task->thread_fn(task, task->ctx);
    PostQueuedCompletionStatus(slot->executor->iocp, 0, (ULONG_PTR)slot, &slot->overlapped);
    return exit_code;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    DWORD thread_id;
    slot->overlapped = (OVERLAPPED){0};
    slot->thread = CreateThread(NULL, 0, executor_thread_wrapper, slot, 0, &thread_id);
}

static char* executor_generate_unique_pipe_name(Allocator* allocator)
{
    Allocator* temp_allocator = allocator_temp();
    char* guid = os_create_guid(temp_allocator, false);
    char* pipe_name = string_from_print(allocator, "\\\\.\\pipe\\%s", guid);
    return pipe_name;
}

static HANDLE executor_create_pipe_pair(ReadPipeContext* ctx)
{
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL};

    char* pipe_name = executor_generate_unique_pipe_name(allocator_temp());

    HANDLE read_pipe = CreateNamedPipeA(
        pipe_name,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE,
        1,
        OUTPUT_BUFFER_SIZE,
        OUTPUT_BUFFER_SIZE,
        0,
        NULL);

    expect(read_pipe != INVALID_HANDLE_VALUE, "failed to create named pipe");

    HANDLE write_pipe = CreateFileA(
        pipe_name,
        GENERIC_WRITE,
        0,
        &sa,
        OPEN_EXISTING,
        0,
        NULL);

    expect(write_pipe != INVALID_HANDLE_VALUE, "failed to open named pipe");

    ctx->read_pipe_handle = read_pipe;
    return write_pipe;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes);

static void executor_read_pipe(ReadPipeContext* ctx)
{
    DWORD bytes;
    ctx->overlapped = (OVERLAPPED){0};
    if (!ReadFile(ctx->read_pipe_handle, ctx->buffer, OUTPUT_BUFFER_SIZE, &bytes, &ctx->overlapped))
    {
        DWORD last_error = GetLastError();
        if (last_error == ERROR_BROKEN_PIPE)
        {
            CloseHandle(ctx->read_pipe_handle);
            ctx->read_pipe_handle = NULL;
        }
        else
        {
            expect(last_error == ERROR_IO_PENDING, "unexpected pipe read error");
        }
    }
}

static HANDLE executor_init_read_pipe_context(ReadPipeContext* ctx, HANDLE iocp, ULONG_PTR key)
{
    if (ctx->write_buffer == NULL)
    {
        ctx->write_buffer = executor_default_write_buffer;
    }

    HANDLE write_pipe = executor_create_pipe_pair(ctx);

    if (!CreateIoCompletionPort(ctx->read_pipe_handle, iocp, key, 0))
    {
        fatal("CreateIoCompletionPort failed!\n");
    }

    executor_read_pipe(ctx);
    return write_pipe;
}

static HANDLE executor_create_nul_file()
{
    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    HANDLE const nul = CreateFileA(
        "NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &security_attributes,
        OPEN_EXISTING,
        0,
        NULL);
    if (nul == INVALID_HANDLE_VALUE)
    {
        fatal("couldn't open nul");
    }
    return nul;
}

static void executor_write_buffer(ReadPipeContext* ctx, char const* buffer, size_t num_bytes)
{
    ctx->write_buffer(ctx->write_buffer_ctx, buffer, num_bytes);
}

static char const* executor_create_process_error_message(DWORD error)
{
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND: return "executable not found";
    case ERROR_PATH_NOT_FOUND: return "executable path not found";
    case ERROR_BAD_EXE_FORMAT: return "bad executable format";
    case ERROR_ACCESS_DENIED: return "executable access denied";
    case ERROR_DIRECTORY: return "executable directory name is invalid";
    case ERROR_INVALID_NAME: return "executable name is invalid";
    case ERROR_INVALID_PARAMETER: return "invalid process parameter";
    default: return "CreateProcessW failed";
    }
}

static void executor_setup_env(Executor* executor, wchar_t* env_block)
{
    if (env_block != executor->current_env_block)
    {
        if (env_block == NULL)
        {
            SetEnvironmentStringsW(executor->default_env_block);
            executor->current_env_block = NULL;
        }
        else
        {
            SetEnvironmentStringsW(env_block);
            executor->current_env_block = env_block;
        }
    }
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    HANDLE stdout_write_pipe_end = executor_init_read_pipe_context(&slot->read_stdout_ctx, slot->executor->iocp, (ULONG_PTR)slot);
    HANDLE stderr_write_pipe_end = executor_init_read_pipe_context(&slot->read_stderr_ctx, slot->executor->iocp, (ULONG_PTR)slot);
    HANDLE const nul = executor_create_nul_file();
    STARTUPINFOW si = {
        .cb = sizeof(STARTUPINFOW),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = nul,
        .hStdOutput = stdout_write_pipe_end,
        .hStdError = stderr_write_pipe_end,
    };
    PROCESS_INFORMATION pi = {0};
    wchar_t* w_cmdline = utf8_to_wchars(allocator_temp(), slot->task->cmdline);
    executor_setup_env(slot->executor, slot->task->env_block);
    if (!CreateProcessW(NULL, w_cmdline, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
    {
        DWORD const error = GetLastError();
        char const* msg = string_from_print(allocator_temp(), "error: %s\n", executor_create_process_error_message(error));
        executor_write_buffer(&slot->read_stderr_ctx, msg, string_length(msg));
        slot->task->exit_code = EXIT_FAILURE;
    }
    else
    {
        CloseHandle(pi.hThread);
    }
    slot->process = pi.hProcess;
    CloseHandle(stdout_write_pipe_end);
    CloseHandle(stderr_write_pipe_end);
}

static void executor_update_process_pipe(ReadPipeContext* read_pipe_ctx)
{
    HANDLE read_pipe = read_pipe_ctx->read_pipe_handle;
    OVERLAPPED* overlapped = &read_pipe_ctx->overlapped;
    DWORD bytes;
    if (!GetOverlappedResult(read_pipe, overlapped, &bytes, TRUE))
    {
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            CloseHandle(read_pipe_ctx->read_pipe_handle);
            read_pipe_ctx->read_pipe_handle = NULL;
            return;
        }
    }
    if (bytes)
    {
        read_pipe_ctx->write_buffer(read_pipe_ctx->write_buffer_ctx, read_pipe_ctx->buffer, bytes);
    }
    executor_read_pipe(read_pipe_ctx);
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    if (slot->read_stderr_ctx.read_pipe_handle == NULL &&
        slot->read_stdout_ctx.read_pipe_handle == NULL)
    {
        return true;
    }
    return false;
}

static void executor_update_slot(ExecutorSlot* slot, OVERLAPPED* overlapped)
{
    if (slot->task->b_thread)
    {
        slot->b_finished = true;
        WaitForSingleObject(slot->thread, INFINITE);
        return;
    }
    if (overlapped == &slot->read_stderr_ctx.overlapped)
    {
        executor_update_process_pipe(&slot->read_stderr_ctx);
        if (slot->read_stderr_ctx.read_pipe_handle == NULL)
        {
            slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
        }
    }
    if (overlapped == &slot->read_stdout_ctx.overlapped)
    {
        executor_update_process_pipe(&slot->read_stdout_ctx);
        if (slot->read_stdout_ctx.read_pipe_handle == NULL)
        {
            slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
        }
    }
    if (executor_is_process_finished(slot))
    {
        slot->b_finished = true;
    }
}

static int executor_get_slot_exit_code(ExecutorSlot* slot)
{
    DWORD exit_code = slot->task->exit_code;
    if (slot->task->b_thread)
    {
        if (slot->thread != NULL)
        {
            GetExitCodeThread(slot->thread, &exit_code);
        }
    }
    else
    {
        if (slot->process)
        {
            GetExitCodeProcess(slot->process, &exit_code);
        }
    }
    return (int)exit_code;
}

static void executor_clear_slot(ExecutorSlot* slot)
{
    if (slot->task->b_thread)
    {
        if (slot->thread)
        {
            CloseHandle(slot->thread);
        }
    }
    else
    {
        if (slot->process)
        {
            CloseHandle(slot->process);
        }
    }
    slot->task = NULL;
}

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    slot->executor = executor;
    if (task->b_thread)
    {
        slot->thread = NULL;
    }
    else
    {
        slot->process = NULL;
    }
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        DWORD bytes_read;
        ULONG_PTR key;
        OVERLAPPED* overlapped;
        if (!GetQueuedCompletionStatus(executor->iocp, &bytes_read, &key, &overlapped, INFINITE))
        {
            expect(GetLastError() == ERROR_BROKEN_PIPE, "GetQueuedCompletionStatus failed");
        }
        ExecutorSlot* slot = (ExecutorSlot*)key;
        executor_update_slot(slot, overlapped);
    }
    uint32_t slot_id = executor_find_finished_slot(executor);
    if (slot_id)
    {
        executor->num_running_tasks -= 1;
        ExecutorSlot* slot = executor_get_slot(executor, slot_id);
        Task* task = slot->task;
        task->exit_code = executor_get_slot_exit_code(slot);
        executor_clear_slot(slot);
        executor_flush(executor);
        return task;
    }
    return NULL;
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    task->env_block = env_block;
}

void executor_set_default_env(Executor* executor, wchar_t* env_block)
{
    executor->default_env_block = env_block;
}
