#include "core/allocator.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct ArenaAllocator ArenaAllocator;

struct ArenaAllocator
{
    void* (*malloc)(Allocator* allocator, size_t size);
    void* (*calloc)(Allocator* allocator, size_t count, size_t size);
    void* (*realloc)(Allocator* allocator, void* ptr, size_t size);
    void (*free)(Allocator* allocator, void* ptr);
    void (*destroy)(Allocator* allocator);
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
    size_t reset_point;
};

#define ARENA_DEFAULT_ALIGNMENT 16

static void* arena_malloc(Allocator* allocator, size_t size)
{
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    uintptr_t current_addr = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t aligned_addr = allocator_align_up(current_addr, ARENA_DEFAULT_ALIGNMENT);
    size_t aligned_offset = (size_t)(aligned_addr - (uintptr_t)arena->buffer);
    if (aligned_offset + size > arena->capacity)
    {
        assert(false && "Arena overflow!");
        return NULL;
    }
    arena->offset = aligned_offset + size;
    return (void*)aligned_addr;
}

static void* arena_calloc(Allocator* instance, size_t count, size_t size)
{
    void* ptr = arena_malloc(instance, count * size);
    memset(ptr, 0, count * size);
    return ptr;
}

static void* arena_realloc(Allocator* instance, void* ptr, size_t size)
{
    void* new_ptr = arena_malloc(instance, size);
    if (ptr != NULL)
    {
        memmove(new_ptr, ptr, size);
    }
    return new_ptr;
}

static void arena_free(Allocator* instance, void* ptr)
{
}

static void arena_destroy(Allocator* instance)
{
    allocator_set_arena_offset(instance, 0);
}

Allocator* allocator_create_arena(void* buffer, size_t buffer_size)
{
    if (buffer_size < sizeof(ArenaAllocator))
    {
        assert(false && "Buffer size is too small");
        return NULL;
    }
    ArenaAllocator* stack_allocator = buffer;
    stack_allocator->malloc = arena_malloc,
    stack_allocator->calloc = arena_calloc,
    stack_allocator->realloc = arena_realloc,
    stack_allocator->free = arena_free,
    stack_allocator->destroy = arena_destroy;
    uintptr_t buffer_start = (uintptr_t)buffer + sizeof(ArenaAllocator);
    uintptr_t aligned_start = allocator_align_up(buffer_start, ARENA_DEFAULT_ALIGNMENT);
    stack_allocator->buffer = (uint8_t*)aligned_start;
    size_t header_and_padding_size = (size_t)(aligned_start - (uintptr_t)buffer);
    stack_allocator->capacity = buffer_size - header_and_padding_size;
    stack_allocator->offset = 0;
    return (Allocator*)stack_allocator;
}

size_t allocator_get_arena_offset(Allocator* allocator)
{
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    return arena->offset;
}

void allocator_set_arena_offset(Allocator* allocator, size_t offset)
{
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    arena->offset = offset;
}