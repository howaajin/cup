#include "core/allocator.h"

#include <stdlib.h>

static void* c_allocator_malloc(Allocator* instance, size_t size)
{
    return malloc(size);
}

static void* c_allocator_calloc(Allocator* instance, size_t count, size_t size)
{
    return calloc(count, size);
}

static void* c_allocator_realloc(Allocator* instance, void* ptr, size_t size)
{
    return realloc(ptr, size);
}

static void c_allocator_free(Allocator* instance, void* ptr)
{
    free(ptr);
}

static void c_allocator_destroy(Allocator* instance)
{
}

Allocator* allocator_c(void)
{
    static Allocator c_allocator_static = {
        .malloc = c_allocator_malloc,
        .calloc = c_allocator_calloc,
        .realloc = c_allocator_realloc,
        .free = c_allocator_free,
        .destroy = c_allocator_destroy,
    };

    return &c_allocator_static;
}
