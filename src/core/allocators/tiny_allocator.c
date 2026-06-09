#include "core/allocator.h"
#include "core/macros.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct TinyNode TinyNode;
typedef struct TinyBlock TinyBlock;
typedef struct TinyAllocator TinyAllocator;

struct TinyBlock
{
    size_t size;
};

struct TinyNode
{
    uint32_t size;
    uint32_t num_allocs;
    uint8_t* p;
    TinyNode* next;
    uint8_t buffer[0];
};

struct TinyAllocator
{
    struct Allocator;
    Allocator* backend;
    TinyNode* head;
    uint32_t limit;
};

static TinyNode* tiny_create_node(Allocator* backend, size_t size)
{
    TinyNode* node = allocator_malloc(backend, size + sizeof(TinyNode));
    node->p = node->buffer;
    node->next = NULL;
    node->size = size ? size : 1;
    node->num_allocs = 0;
    return node;
}

static void tiny_free(Allocator* allocator, void* ptr)
{
    TinyAllocator* a = (TinyAllocator*)allocator;
    if (!ptr)
    {
        return;
    }
    TinyNode* node = a->head;
Loop:
    if ((uint8_t*)ptr >= node->buffer && (uint8_t*)ptr < node->buffer + node->size)
    {
        node->num_allocs -= 1;
        if (node->num_allocs == 0)
        {
            node->p = node->buffer;
        }
    }
    else if (node->next)
    {
        node = node->next;
        goto Loop;
    }
    else
    {
        allocator_free(a->backend, ptr);
    }
}

static void* tiny_malloc(Allocator* allocator, size_t size)
{
    TinyAllocator* a = (TinyAllocator*)allocator;
    size_t adj_size = allocator_align_up(size, sizeof(size_t));
    TinyNode* node = a->head;
    while (true)
    {
        if (node->p - node->buffer + adj_size + sizeof(TinyBlock) <= node->size)
        {
            TinyBlock* block = (TinyBlock*)node->p;
            void* result = block + 1;
            node->p += adj_size + sizeof(TinyBlock);
            block->size = adj_size;
            node->num_allocs += 1;
            return result;
        }
        else if (node->next)
        {
            node = node->next;
        }
        else
        {
            TinyNode* next = a->head ? a->head : node;
            node = tiny_create_node(a->backend, next->size * 2);
            node->next = next;
            a->head = node;
        }
    }
}

static void* tiny_calloc(Allocator* allocator, size_t count, size_t size)
{
    void* ptr = tiny_malloc(allocator, count * size);
    memset(ptr, 0, count * size);
    return ptr;
}

static void* tiny_realloc(Allocator* allocator, void* ptr, size_t size)
{
    if (size == 0)
    {
        tiny_free(allocator, ptr);
        return NULL;
    }
    if (ptr == NULL)
    {
        return tiny_malloc(allocator, size);
    }
    TinyAllocator* a = (TinyAllocator*)allocator;
    size_t adj_size = allocator_align_up(size, sizeof(size_t));
    TinyNode* node = a->head;
    bool is_own;
Loop:
    is_own = (uint8_t*)ptr >= node->buffer && (uint8_t*)ptr < node->buffer + node->size;
    if (is_own && size <= a->limit)
    {
        TinyBlock* old_block = (TinyBlock*)ptr - 1;
        size_t old_size = old_block->size;
        if (node->p - old_size == ptr)
        {
            node->p -= (old_size + sizeof(TinyBlock));
        }
        if (node->p - node->buffer + adj_size + sizeof(TinyBlock) < node->size)
        {
            TinyBlock* block = (TinyBlock*)node->p;
            void* result = block + 1;
            node->p += adj_size + sizeof(TinyBlock);
            if (ptr != result)
            {
                size_t copy_size = old_size;
                if (copy_size > adj_size) copy_size = adj_size;
                memcpy(result, ptr, copy_size);
            }
            block->size = adj_size;
            return result;
        }
        else
        {
            node->num_allocs -= 1;
            void* result = tiny_malloc(allocator, size);
            size_t copy_size = old_size;
            if (copy_size > size) copy_size = size;
            memcpy(result, ptr, copy_size);
            return result;
        }
    }
    if (is_own)
    {
        node->num_allocs -= 1;
        if (node->num_allocs == 0)
        {
            node->p = node->buffer;
        }
        void* result = allocator_malloc(a->backend, size);
        TinyBlock* old_block = (TinyBlock*)ptr - 1;
        size_t copy_size = old_block->size;
        if (copy_size > size) copy_size = size;
        memcpy(result, ptr, copy_size);
        return result;
    }
    else if (node->next)
    {
        node = node->next;
        goto Loop;
    }
    else
    {
        return a->backend->realloc(a->backend, ptr, size);
    }
}

static void tiny_destroy(Allocator* allocator)
{
    TinyAllocator* a = (TinyAllocator*)allocator;
    allocator_destroy(a->backend);
}

void tiny_reset(Allocator* allocator)
{
    TinyAllocator* a = (TinyAllocator*)allocator;
    TinyNode* node = a->head;
    while (node)
    {
        node->p = node->buffer;
        node->num_allocs = 0;
        node = node->next;
    }
}

Allocator* allocator_create_tiny(uint32_t limit, uint32_t size)
{
    Allocator* backend = allocator_create_chained();
    TinyAllocator* a = allocator_malloc(backend, sizeof(TinyAllocator));
    expect(a, "allocation failed");
    a->backend = backend;
    a->head = tiny_create_node(backend, size);
    a->limit = limit;
    a->malloc = tiny_malloc;
    a->calloc = tiny_calloc;
    a->realloc = tiny_realloc;
    a->free = tiny_free;
    a->destroy = tiny_destroy;
    return (Allocator*)a;
}