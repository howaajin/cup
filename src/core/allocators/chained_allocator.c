#include "core/allocator.h"
#include "core/compat_threads.h" // IWYU pragma: keep
#include "core/macros.h"

#include <assert.h>

typedef struct ChainedBlock ChainedBlock;
typedef struct ChainedAllocator ChainedAllocator;

struct ChainedBlock
{
    ChainedBlock* next;
    ChainedBlock* prev;
};

struct ChainedAllocator
{
    void* (*malloc)(Allocator* allocator, size_t size);
    void* (*calloc)(Allocator* allocator, size_t count, size_t size);
    void* (*realloc)(Allocator* allocator, void* ptr, size_t size);
    void (*free)(Allocator* allocator, void* ptr);
    void (*destroy)(Allocator* allocator);
    Allocator* backend;
    ChainedBlock* head;
    mtx_t mutex;
};

static void* chained_allocator_malloc(Allocator* allocator, size_t size)
{
    ChainedAllocator* chained_allocator = (ChainedAllocator*)allocator;
    ChainedBlock* block = allocator_malloc(chained_allocator->backend, sizeof(ChainedBlock) + size);
    expect(block, "allocation failed");
    block->next = NULL;
    block->prev = NULL;
    mtx_lock(&chained_allocator->mutex);
    block->next = chained_allocator->head;
    if (chained_allocator->head)
    {
        chained_allocator->head->prev = block;
    }
    chained_allocator->head = block;
    mtx_unlock(&chained_allocator->mutex);
    return block + 1;
}

static void* chained_allocator_calloc(Allocator* allocator, size_t count, size_t size)
{
    ChainedAllocator* chained_allocator = (ChainedAllocator*)allocator;
    size_t num_bytes = count * size + sizeof(ChainedBlock);
    ChainedBlock* block = allocator_calloc(chained_allocator->backend, num_bytes, 1);
    expect(block, "allocation failed");
    block->next = NULL;
    block->prev = NULL;
    mtx_lock(&chained_allocator->mutex);
    block->next = chained_allocator->head;
    if (chained_allocator->head)
    {
        chained_allocator->head->prev = block;
    }
    chained_allocator->head = block;
    mtx_unlock(&chained_allocator->mutex);
    return block + 1;
}

static void* chained_allocator_realloc(Allocator* allocator, void* ptr, size_t size)
{
    ChainedAllocator* chained_allocator = (ChainedAllocator*)allocator;
    mtx_lock(&chained_allocator->mutex);
    ChainedBlock* old_block = ptr == NULL ? NULL : (ChainedBlock*)ptr - 1;
    ChainedBlock* new_block = allocator_realloc(chained_allocator->backend, old_block, sizeof(ChainedBlock) + size);
    expect(new_block, "reallocation failed");
    if (ptr != NULL)
    {
        if (new_block->prev)
        {
            new_block->prev->next = new_block;
        }
        else
        {
            chained_allocator->head = new_block;
        }
        if (new_block->next)
        {
            new_block->next->prev = new_block;
        }
    }
    else
    {
        new_block->next = chained_allocator->head;
        new_block->prev = NULL;
        if (chained_allocator->head)
        {
            chained_allocator->head->prev = new_block;
        }
        chained_allocator->head = new_block;
    }
    mtx_unlock(&chained_allocator->mutex);
    return new_block + 1;
}

static void chained_allocator_free(Allocator* allocator, void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    ChainedAllocator* chained_allocator = (ChainedAllocator*)allocator;
    ChainedBlock* block = (ChainedBlock*)ptr - 1;
    mtx_lock(&chained_allocator->mutex);
    if (block->prev)
    {
        block->prev->next = block->next;
    }
    else
    {
        chained_allocator->head = block->next;
    }
    if (block->next)
    {
        block->next->prev = block->prev;
    }
    mtx_unlock(&chained_allocator->mutex);
    allocator_free(chained_allocator->backend, block);
}

static void chained_allocator_destroy(Allocator* allocator)
{
    ChainedAllocator* chained_allocator = (ChainedAllocator*)allocator;
    mtx_lock(&chained_allocator->mutex);
    ChainedBlock* current = chained_allocator->head;
    while (current != NULL)
    {
        ChainedBlock* next = current->next;
        allocator_free(chained_allocator->backend, current);
        current = next;
    }
    mtx_unlock(&chained_allocator->mutex);
    mtx_destroy(&chained_allocator->mutex);
    allocator_free(chained_allocator->backend, chained_allocator);
}

Allocator* allocator_create_chained(void)
{
    Allocator* c_allocator = allocator_c();
    ChainedAllocator* allocator = allocator_malloc(c_allocator, sizeof(ChainedAllocator));
    expect(allocator, "allocation failed");
    *allocator = (ChainedAllocator){
        .malloc = chained_allocator_malloc,
        .calloc = chained_allocator_calloc,
        .realloc = chained_allocator_realloc,
        .free = chained_allocator_free,
        .destroy = chained_allocator_destroy,
        .backend = c_allocator,
    };
    if (mtx_init(&allocator->mutex, mtx_plain) != 0)
    {
        allocator_free(c_allocator, allocator);
        return NULL;
    }
    return (Allocator*)allocator;
}
