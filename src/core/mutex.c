#include "core/allocator.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef void* Mutex;

#if defined(_WIN32)

int os_mtx_init(Mutex* mtx)
{
    *mtx = allocator_calloc(allocator_c(), 1, sizeof(CRITICAL_SECTION));
    CRITICAL_SECTION* ritical_section = *mtx;
    InitializeCriticalSection(ritical_section);
    return 0;
}

void os_mtx_destroy(Mutex* mtx)
{
    DeleteCriticalSection(*mtx);
    allocator_free(allocator_c(), *mtx);
    *mtx = NULL;
}

int os_mtx_lock(Mutex* mtx)
{
    EnterCriticalSection(*mtx);
    return 0;
}

int os_mtx_unlock(Mutex* mtx)
{
    LeaveCriticalSection(*mtx);
    return 0;
}

#else

int os_mtx_init(Mutex* mtx)
{
    *mtx = allocator_calloc(allocator_c(), 1, sizeof(pthread_mutex_t));
    pthread_mutex_t* pthread_mutex = *mtx;
    if (pthread_mutex_init(pthread_mutex, NULL) == 0)
    {
        return 0;
    }
    return 1;
}

void os_mtx_destroy(Mutex* mtx)
{
    pthread_mutex_destroy(*mtx);
    allocator_free(allocator_c(), *mtx);
    *mtx = NULL;
}

int os_mtx_lock(Mutex* mtx)
{
    if (pthread_mutex_lock(*mtx) == 0)
    {
        return 0;
    }
    return 1;
}

int os_mtx_unlock(Mutex* mtx)
{
    if (pthread_mutex_unlock(*mtx) == 0)
    {
        return 0;
    }
    return 1;
}

#endif
