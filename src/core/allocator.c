
#include "core/allocator.h"

#include "core/os.h"

#include <stddef.h>
#include <stdint.h>

static tss_t temp_allocator_key;
static once_flag temp_allocator_once = ONCE_FLAG_INIT;

static void cleanup_temp_allocator(void* data)
{
    Allocator* a = (Allocator*)data;
    allocator_destroy(a);
}

static void init_temp_key(void)
{
    tss_create(&temp_allocator_key, cleanup_temp_allocator);
}

Allocator* allocator_temp(void)
{
    call_once(&temp_allocator_once, init_temp_key);
    Allocator* a = (Allocator*)tss_get(temp_allocator_key);
    if (a == NULL)
    {
        a = allocator_create_tiny(4096, 4096 * 128);
        tss_set(temp_allocator_key, a);
    }
    return a;
}

void allocator_reset_temp(void)
{
    void tiny_reset(Allocator * allocator);

    Allocator* a = (Allocator*)tss_get(temp_allocator_key);
    if (a)
    {
        tiny_reset(a);
    }
}
