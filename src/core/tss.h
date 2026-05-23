#pragma once

#include "core/platform.h"

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define OS_API_CALLCONV __stdcall

typedef DWORD os_tss_t;
typedef INIT_ONCE os_once_flag;
#define OS_ONCE_FLAG_INIT INIT_ONCE_STATIC_INIT
static BOOL CALLBACK windows_once_callback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
    void (*func)(void) = (void (*)(void))Parameter;
    func();
    return TRUE;
}
static inline void os_call_once(os_once_flag* flag, void (*func)(void))
{
    InitOnceExecuteOnce((void*)flag, windows_once_callback, (void*)func, NULL);
}
static inline int os_tss_create(os_tss_t* key, void(__stdcall* dtor)(void*))
{
    *key = FlsAlloc((PFLS_CALLBACK_FUNCTION)dtor);
    return (*key == FLS_OUT_OF_INDEXES) ? 0 : 1;
}
static inline void* os_tss_get(os_tss_t key)
{
    return FlsGetValue(key);
}
static inline int os_tss_set(os_tss_t key, void* val)
{
    return FlsSetValue(key, val) ? 1 : 0;
}
static inline void os_tss_delete(os_tss_t key)
{
    FlsFree(key);
}

#else
#include <pthread.h>
typedef pthread_key_t os_tss_t;
typedef pthread_once_t os_once_flag;
#define OS_ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#define OS_API_CALLCONV

static inline int os_tss_create(os_tss_t* key, void (*dtor)(void*))
{
    return (pthread_key_create(key, dtor) == 0) ? 1 : 0;
}
static inline void* os_tss_get(os_tss_t key)
{
    return pthread_getspecific(key);
}
static inline int os_tss_set(os_tss_t key, void* val)
{
    return (pthread_setspecific(key, val) == 0) ? 1 : 0;
}
static inline void os_tss_delete(os_tss_t key)
{
    pthread_key_delete(key);
}
static inline void os_call_once(os_once_flag* flag, void (*func)(void))
{
    pthread_once(flag, func);
}
#endif
