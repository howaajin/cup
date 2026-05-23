#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct Allocator Allocator;

struct Allocator
{
    void* (*malloc)(Allocator* allocator, size_t size);
    void* (*calloc)(Allocator* allocator, size_t count, size_t size);
    void* (*realloc)(Allocator* allocator, void* ptr, size_t size);
    void (*free)(Allocator* allocator, void* ptr);
    void (*destroy)(Allocator* allocator);
};

Allocator* allocator_c(void);
Allocator* allocator_create_chained(void);
Allocator* allocator_create_tiny(uint32_t limit, uint32_t size);

Allocator* allocator_temp(void);
void allocator_reset_temp(void);

Allocator* allocator_create_arena(void* buffer, size_t buffer_size);
size_t allocator_get_arena_offset(Allocator* allocator);
void allocator_set_arena_offset(Allocator* allocator, size_t offset);

void* allocator_virtual_alloc(void* base_address, size_t size);
void allocator_virtual_free(void* base_address, size_t size);

static inline size_t allocator_align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline void* allocator_malloc(Allocator* allocator, size_t size)
{
    return allocator->malloc(allocator, size);
}
static inline void* allocator_calloc(Allocator* allocator, size_t count, size_t size)
{
    return allocator->calloc(allocator, count, size);
}
static inline void* allocator_realloc(Allocator* allocator, void* ptr, size_t size)
{
    return allocator->realloc(allocator, ptr, size);
}
static inline void allocator_free(Allocator* allocator, void* ptr)
{
    allocator->free(allocator, ptr);
}
static inline void allocator_destroy(Allocator* allocator)
{
    allocator->destroy(allocator);
}

#define allocator_arena_from_buffer(buffer) allocator_create_arena(buffer, sizeof(buffer))

// alloca
#ifdef _WIN32
    #include <malloc.h>
#else
    #include <alloca.h>
#endif
#define allocator_arena_from_alloca(size) allocator_create_arena(alloca(size), (size))
