#include "cup/executor/executor_mac.h"
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
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static void kq_update_event(int kq, int fd, int filter, int flags, void* udata)
{
    struct kevent sev;
    EV_SET(&sev, fd, filter, flags, 0, 0, udata);
    if (kevent(kq, &sev, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent register");
    }
}

void executor_platform_init(Executor* executor)
{
    executor->kq_fd = kqueue();
    expect(executor->kq_fd != -1, "failed to create kqueue");
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        slot->ctx_thread = KQ_CTX_THREAD;
        slot->ctx_stdout = KQ_CTX_STDOUT;
        slot->ctx_stderr = KQ_CTX_STDERR;
        slot->kq_fd = executor->kq_fd;
    }
}

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (task->b_thread)
    {
        slot->thread_done_pipe[0] = -1;
        slot->thread_done_pipe[1] = -1;
    }
    else
    {
        slot->pid = 0;
    }
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    fatal("Not implemented on macOS.");
}

void executor_platform_destroy(Executor* executor)
{
    close(executor->kq_fd);
}

void executor_read_pipe(ReadPipeContext* ctx)
{
    char buffer[OUTPUT_BUFFER_SIZE];
    ssize_t bytes = read(ctx->read_pipe, buffer, OUTPUT_BUFFER_SIZE);
    if (bytes > 0)
    {
        ctx->write_buffer(ctx->write_buffer_ctx, buffer, bytes);
    }
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    return (slot->read_stdout_ctx.read_pipe == -1 && slot->read_stderr_ctx.read_pipe == -1);
}

void executor_update_process(ExecutorSlot* slot, struct kevent* e, ReadPipeContext* ctx)
{
    executor_read_pipe(ctx);

    if (e->flags & EV_EOF)
    {
        kq_update_event(slot->kq_fd, ctx->read_pipe, EVFILT_READ, EV_DELETE, NULL);
        close(ctx->read_pipe);
        ctx->read_pipe = -1;
    }

    if (executor_is_process_finished(slot))
    {
        int status;
        pid_t pid = waitpid(slot->pid, &status, 0);
        if (pid != -1)
        {
            slot->task->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
            slot->b_finished = true;
            slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
            slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
        }
    }
}

void executor_update_thread(ExecutorSlot* slot, struct kevent* e)
{
    char dummy;
    read(slot->thread_done_pipe[0], &dummy, 1);

    void* return_value;
    pthread_join(slot->thread_id, &return_value);

    kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_DELETE, NULL);
    close(slot->thread_done_pipe[0]);
    close(slot->thread_done_pipe[1]);
    slot->thread_done_pipe[0] = -1;
    slot->thread_done_pipe[1] = -1;

    slot->b_finished = true;
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        struct kevent e;
        int n = kevent(executor->kq_fd, NULL, 0, &e, 1, NULL);
        if (n <= 0) return NULL;

        KqueueContextType* ctx_type = (KqueueContextType*)e.udata;
        if (*ctx_type == KQ_CTX_STDERR)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stderr));
            executor_update_process(slot, &e, &slot->read_stderr_ctx);
        }
        else if (*ctx_type == KQ_CTX_STDOUT)
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

static void* executor_callback_thread_wrapper(void* ctx)
{
    ExecutorSlot* slot = ctx;
    Task* task = slot->task;
    task->exit_code = EXIT_FAILURE;
    task->exit_code = task->thread_fn(task, task->ctx);

    char signal_value = 1;
    ssize_t s = write(slot->thread_done_pipe[1], &signal_value, 1);
    (void)s;
    return NULL;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    if (pipe(slot->thread_done_pipe) == -1)
    {
        perror("pipe failed");
        return;
    }
    fcntl(slot->thread_done_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(slot->thread_done_pipe[1], F_SETFL, O_NONBLOCK);

    kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_ADD, &slot->ctx_thread);

    int result = pthread_create(&slot->thread_id, NULL, executor_callback_thread_wrapper, slot);
    expect(result == 0, "pthread_create failed");
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    int pipe_stdout_fds[2];
    int pipe_stderr_fds[2];
    if (pipe(pipe_stdout_fds) == -1 || pipe(pipe_stderr_fds) == -1)
    {
        fprintf(stderr, "pipe failed\n");
        exit(EXIT_FAILURE);
    }
    fcntl(pipe_stdout_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_stderr_fds[0], F_SETFL, O_NONBLOCK);

    slot->read_stdout_ctx.read_pipe = pipe_stdout_fds[0];
    slot->read_stderr_ctx.read_pipe = pipe_stderr_fds[0];

    kq_update_event(slot->kq_fd, slot->read_stdout_ctx.read_pipe, EVFILT_READ, EV_ADD, &slot->ctx_stdout);
    kq_update_event(slot->kq_fd, slot->read_stderr_ctx.read_pipe, EVFILT_READ, EV_ADD, &slot->ctx_stderr);

    pid_t pid = fork();
    if (pid == -1) exit(EXIT_FAILURE);
    if (pid == 0)
    {
        close(pipe_stdout_fds[0]);
        close(pipe_stderr_fds[0]);
        dup2(pipe_stdout_fds[1], STDOUT_FILENO);
        dup2(pipe_stderr_fds[1], STDERR_FILENO);
        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);

        execl("/bin/sh", "sh", "-c", slot->task->cmdline, NULL);
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
    if (slot->read_stdout_ctx.read_pipe != -1)
    {
        kq_update_event(slot->kq_fd, slot->read_stdout_ctx.read_pipe, EVFILT_READ, EV_DELETE, NULL);
        close(slot->read_stdout_ctx.read_pipe);
        slot->read_stdout_ctx.read_pipe = -1;
    }
    if (slot->read_stderr_ctx.read_pipe != -1)
    {
        kq_update_event(slot->kq_fd, slot->read_stderr_ctx.read_pipe, EVFILT_READ, EV_DELETE, NULL);
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
    if (slot->thread_done_pipe[0] != -1)
    {
        kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_DELETE, NULL);
        close(slot->thread_done_pipe[0]);
        close(slot->thread_done_pipe[1]);
        slot->thread_done_pipe[0] = -1;
        slot->thread_done_pipe[1] = -1;
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
