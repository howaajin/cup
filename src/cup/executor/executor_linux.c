#include "cup/executor/executor_linux.h"
#include "cup/executor/executor.h"

#include "core/macros.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

void executor_platform_init(Executor* executor)
{
    executor->epoll_fd = epoll_create1(0);
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        slot->ctx_thread = EPOLL_CTX_THREAD;
        slot->ctx_stdout = EPOLL_CTX_STDOUT;
        slot->ctx_stderr = EPOLL_CTX_STDERR;
        slot->epoll_fd = executor->epoll_fd;
    }
}

void executor_platform_destroy(Executor* executor)
{
    close(executor->epoll_fd);
}

void executor_read_pipe(ReadPipeContext* ctx)
{
    char buffer[OUTPUT_BUFFER_SIZE];
    ssize_t bytes = read(ctx->read_pipe, buffer, OUTPUT_BUFFER_SIZE);
    if (bytes)
    {
        ctx->write_buffer(ctx->write_buffer_ctx, buffer, bytes);
    }
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    if (slot->read_stdout_ctx.read_pipe == -1 && slot->read_stderr_ctx.read_pipe == -1)
    {
        return true;
    }
    return false;
}

void executor_update_process(ExecutorSlot* slot, struct epoll_event* e, ReadPipeContext* ctx)
{
    if (e->events & EPOLLIN)
    {
        executor_read_pipe(ctx);
    }
    if (e->events & (EPOLLHUP | EPOLLERR))
    {
        if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, ctx->read_pipe, NULL) == -1)
        {
            perror("epoll_ctl DEL");
        }
        close(ctx->read_pipe);
        ctx->read_pipe = -1;
    }
    if (executor_is_process_finished(slot))
    {
        int status;
        pid_t pid = waitpid(slot->pid, &status, 0);
        if (pid == -1)
        {
            fatal("waitpid failed");
        }
        slot->task->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
        slot->b_finished = true;
        slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
        slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
    }
}

void executor_update_thread(ExecutorSlot* slot, struct epoll_event* e)
{
    if (e->events & EPOLLIN)
    {
        uint64_t signal_value;
        ssize_t n = read(slot->event_fd, &signal_value, sizeof(signal_value));
        if (n == -1)
        {
            perror("read event_fd");
        }
        void* return_value;
        pthread_join(slot->thread_id, &return_value);
        if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, slot->event_fd, NULL) == -1)
        {
            perror("epoll_ctl DEL");
        }
        close(slot->event_fd);
        slot->event_fd = -1;
        slot->b_finished = true;
    }
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        struct epoll_event e;
        int n = epoll_wait(executor->epoll_fd, &e, 1, -1);
        if (n != 1)
        {
            return NULL;
        }
        EpollContextType* ctx_type = (EpollContextType*)e.data.ptr;
        if (*ctx_type == EPOLL_CTX_STDERR)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stderr));
            executor_update_process(slot, &e, &slot->read_stderr_ctx);
        }
        else if (*ctx_type == EPOLL_CTX_STDOUT)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stdout));
            if (slot->task->b_thread)
            {
                executor_update_thread(slot, &e);
            }
            else
            {
                executor_update_process(slot, &e, &slot->read_stdout_ctx);
            }
        }
    }
    uint32_t slot_id = executor_find_finished_slot(executor);
    if (slot_id)
    {
        executor->num_running_tasks -= 1;
        ExecutorSlot* slot = executor_get_slot(executor, slot_id);
        Task* task = slot->task;
        slot->task = NULL;
        executor_flush(executor);
        return task;
    }
    return NULL;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes);

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (task->b_thread)
    {
        slot->event_fd = -1;
    }
    else
    {
        slot->pid = 0;
    }
}

static void* executor_callback_thread_warpper(void* ctx)
{
    ExecutorSlot* slot = ctx;
    Task* task = slot->task;
    task->exit_code = EXIT_FAILURE;
    task->exit_code = task->thread_fn(task, task->ctx);
    uint64_t signal_value = 1;
    ssize_t written = write(slot->event_fd, &signal_value, sizeof(signal_value));
    if (written == -1)
    {
        perror("write event_fd");
    }
    return NULL;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    ExecutorSlot* s = (ExecutorSlot*)slot;
    s->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    expect(s->event_fd != -1, "eventfd creation failed");
    struct epoll_event e = {.events = EPOLLIN, .data = {.ptr = &s->ctx_thread}};
    if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, s->event_fd, &e) == -1)
    {
        fatal("epoll_ctl ADD failed");
    }
    pthread_create(&s->thread_id, NULL, executor_callback_thread_warpper, s);
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    int pipe_stdout_fds[2];
    int pipe_stderr_fds[2];
    if (pipe(pipe_stdout_fds) == -1 || pipe(pipe_stderr_fds) == -1)
    {
        printf("pipe failed\n");
        exit(EXIT_FAILURE);
    }
    fcntl(pipe_stdout_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_stderr_fds[0], F_SETFL, O_NONBLOCK);

    slot->read_stdout_ctx.read_pipe = pipe_stdout_fds[0];
    slot->read_stderr_ctx.read_pipe = pipe_stderr_fds[0];
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = {.ptr = &slot->ctx_stdout},
    };
    epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, slot->read_stdout_ctx.read_pipe, &ev);
    ev.data = (epoll_data_t){.ptr = &slot->ctx_stderr};
    epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, slot->read_stderr_ctx.read_pipe, &ev);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        close(pipe_stdout_fds[0]);
        close(pipe_stderr_fds[0]);

        dup2(pipe_stdout_fds[1], STDOUT_FILENO);
        dup2(pipe_stderr_fds[1], STDERR_FILENO);

        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);

        execl("/bin/sh", "sh", "-c", slot->task->cmdline, NULL);
        fprintf(stderr, "exec error: %s\n", slot->task->cmdline);
        perror("execv failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);
        slot->pid = pid;
    }
}

void executor_force_kill_task_process(ExecutorSlot* slot)
{
    expect(slot->task, "slot->task is NULL");
    if (slot->read_stdout_ctx.read_pipe != -1)
    {
        close(slot->read_stdout_ctx.read_pipe);
        slot->read_stdout_ctx.read_pipe = -1;
    }
    if (slot->read_stderr_ctx.read_pipe != -1)
    {
        close(slot->read_stderr_ctx.read_pipe);
        slot->read_stderr_ctx.read_pipe = -1;
    }
    if (slot->pid)
    {
        kill(slot->pid, SIGKILL);
        waitpid(slot->pid, NULL, 0);
    }
}

void executor_force_kill_task_thread(ExecutorSlot* slot)
{
    expect(slot->task, "slot->task is NULL");
    if (slot->event_fd != -1)
    {
        int result = epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, slot->event_fd, NULL);
        if (result == -1)
        {
            if (errno != EBADF && errno != ENOENT)
            {
                perror("epoll_ctl DEL");
            }
        }
        close(slot->event_fd);
        slot->event_fd = -1;
    }
}

void executor_force_kill_task(ExecutorSlot* slot)
{
    if (slot->task)
    {
        if (slot->task->b_thread)
        {
            executor_force_kill_task_thread(slot);
        }
        else
        {
            executor_force_kill_task_process(slot);
        }
    }
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    fatal("Not implemented on Linux.");
}
