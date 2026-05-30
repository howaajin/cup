#pragma once

#if defined(__APPLE__) || defined(__STDC_NO_THREADS__)

#include <errno.h>
#include <pthread.h>

typedef pthread_mutex_t mtx_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;
#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
enum
{
    mtx_plain = 0,
    mtx_recursive = 1,
    mtx_timed = 2
};

#define thrd_success 0
#define thrd_busy 1
#define thrd_error -1
static inline int mtx_init(mtx_t* mtx, int type)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    if (type & mtx_recursive)
    {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }

    int r = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return r == 0 ? thrd_success : thrd_error;
}

static inline void mtx_destroy(mtx_t* mtx)
{
    pthread_mutex_destroy(mtx);
}

static inline int mtx_lock(mtx_t* mtx)
{
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_trylock(mtx_t* mtx)
{
    int r = pthread_mutex_trylock(mtx);
    if (r == 0) return thrd_success;
    if (r == EBUSY) return thrd_busy;
    return thrd_error;
}

static inline int mtx_unlock(mtx_t* mtx)
{
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int tss_create(tss_t* key, void (*dtor)(void*))
{
    return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

static inline void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int tss_set(tss_t key, void* val)
{
    return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}

static inline void tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

static inline void call_once(once_flag* flag, void (*func)(void))
{
    pthread_once(flag, func);
}

#else
#include <threads.h>
#endif
