#pragma once

typedef void* Mutex;

int os_mtx_init(Mutex* mtx);
void os_mtx_destroy(Mutex* mtx);
int os_mtx_lock(Mutex* mtx);
int os_mtx_unlock(Mutex* mtx);