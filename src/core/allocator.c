
#include "core/allocator.h"

#include "core/tss.h"

#include <stddef.h>
#include <stdint.h>

static os_tss_t temp_allocator_key;
static os_once_flag temp_allocator_once = OS_ONCE_FLAG_INIT;

static void OS_API_CALLCONV cleanup_temp_allocator(void* data)
{
    Allocator* a = (Allocator*)data;
    allocator_destroy(a);
}

static void init_temp_key(void)
{
    os_tss_create(&temp_allocator_key, cleanup_temp_allocator);
}

Allocator* allocator_temp(void)
{
    os_call_once(&temp_allocator_once, init_temp_key);
    Allocator* a = (Allocator*)os_tss_get(temp_allocator_key);
    if (a == NULL)
    {
        a = allocator_create_tiny(4096, 4096 * 128);
        os_tss_set(temp_allocator_key, a);
    }
    return a;
}

void allocator_reset_temp(void)
{
    void tiny_reset(Allocator * allocator);

    Allocator* a = (Allocator*)os_tss_get(temp_allocator_key);
    if (a)
    {
        tiny_reset(a);
    }
}
