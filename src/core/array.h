#pragma once

#include "core/allocator.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Array Array;
struct Array
{
    size_t size;
    size_t capacity;
};

#define array_header(a) ((Array*)(a) - 1)
#define array_size_lvalue(a) array_header(a)->size
#define array_size(a) ((a) ? array_size_lvalue(a) : 0)
#define array_capacity_lvalue(a) array_header(a)->capacity
#define array_capacity(a) ((a) ? array_header(a)->capacity : 0)

#define array_bytes(a) (sizeof((a)[0]) * array_size(a))
#define array_last(a) (a)[array_size_lvalue(a) - 1]
#define array_from_stack(type, n) alloca((n) * sizeof(type))
#define array_pop(a) (a)[--array_size_lvalue(a)]
#define array_remove_unordered(a, i) (a)[i] = array_pop(a)

#define array_new(allocator, type, size, capacity) array_new_impl(allocator, sizeof(type), size, capacity)
#define array_reserve(allocator, a, n) array_reserve_impl(allocator, (void**)&(a), sizeof((a)[0]), (n))
#define array_resize(allocator, a, n) array_resize_impl(allocator, (void**)&(a), sizeof((a)[0]), (n))
#define array_shrink_to_fit(allocator, a) array_reserve_impl(allocator, (void**)&(a), sizeof((a)[0]), array_size(a))

#define array_push(allocator, a, item, ...)                                                  \
    do                                                                                       \
    {                                                                                        \
        size_t array_push_old_size = array_size(a);                                          \
        array_resize_impl(allocator, (void**)&(a), sizeof((a)[0]), array_push_old_size + 1); \
        (a)[array_push_old_size] = (item, ##__VA_ARGS__);                                    \
    } while (0)

#define array_push_v(allocator, a, v, n) array_push_v_impl(allocator, (void**)&(a), sizeof((a)[0]), (v), (n))
#define array_insert(allocator, a, i, n) array_insert_impl(allocator, (void**)&(a), sizeof((a)[0]), (i), (n))
#define array_move(a, b) array_move_impl((void**)&(a), (void**)&(b))
#define array_free(allocator, a) array_free_impl(allocator, (void**)&(a))
#define array_remove_n(a, i, n) array_remove_n_impl((void*)(a), sizeof((a)[0]), (i), (n))

#define array_find(a, pred, args) array_find_impl((void*)(a), sizeof((a)[0]), (pred), (args))
#define array_compact(a, pred, args) array_compact_impl((void*)(a), sizeof((a)[0]), (pred), (args))

static inline size_t array_calc_capacity(size_t old_capacity, size_t new_size)
{
    size_t new_capacity = old_capacity;
    if (new_size > old_capacity) new_capacity = new_size;
    if (new_capacity <= old_capacity) return old_capacity;
    if (new_capacity < 2 * old_capacity) new_capacity = 2 * old_capacity;
    if (new_capacity < 4) return 4;
    return new_capacity;
}

static inline void* array_new_impl(Allocator* allocator, size_t item_size, size_t size, size_t capacity)
{
    size_t new_capacity = array_calc_capacity(0, size);
    if (new_capacity < capacity)
    {
        new_capacity = capacity;
    }
    Array* a = allocator_malloc(allocator, sizeof(Array) + item_size * new_capacity);
    assert(a);
    a->capacity = new_capacity;
    a->size = size;
    return a + 1;
}

static inline void array_reserve_impl(Allocator* allocator, void** ptr, size_t item_size, size_t n)
{
    size_t bytes = n * item_size + sizeof(Array);
    Array* h = *ptr ? array_header(*ptr) : NULL;
    h = allocator_realloc(allocator, h, bytes);
    h->capacity = n;
    if (!*ptr) h->size = 0;
    *ptr = h + 1;
}

static inline void array_resize_impl(Allocator* allocator, void** ptr, size_t item_size, size_t n)
{
    size_t old_capacity = *ptr ? array_capacity(*ptr) : 0;
    size_t new_capacity = array_calc_capacity(old_capacity, n);
    if (new_capacity != old_capacity)
    {
        array_reserve_impl(allocator, ptr, item_size, new_capacity);
    }
    if (*ptr)
    {
        array_size_lvalue(*ptr) = n;
    }
}

static inline void array_push_v_impl(Allocator* allocator, void** ptr, size_t item_size, void const* src, size_t n)
{
    size_t old_size = *ptr ? array_size(*ptr) : 0;
    array_reserve_impl(allocator, ptr, item_size, old_size + n);
    memcpy((char*)*ptr + old_size * item_size, src, n * item_size);
    array_size_lvalue(*ptr) = old_size + n;
}

static inline void array_insert_impl(Allocator* allocator, void** ptr, size_t item_size, size_t index, size_t n)
{
    size_t old_size = *ptr ? array_size(*ptr) : 0;
    assert(index <= old_size);
    array_resize_impl(allocator, ptr, item_size, old_size + n);
    size_t bytes_to_move = (old_size - index) * item_size;
    if (bytes_to_move)
    {
        memmove((char*)*ptr + (index + n) * item_size, (char*)*ptr + index * item_size, bytes_to_move);
    }
}

static inline void array_free_impl(Allocator* allocator, void** ptr)
{
    allocator_free(allocator, *ptr ? array_header(*ptr) : NULL);
    *ptr = NULL;
}

static inline void array_remove_n_impl(void* a, size_t item_size, size_t index, size_t n)
{
    size_t old_size = a ? array_size(a) : 0;
    if (old_size >= n)
    {
        memmove((char*)a + index * item_size, (char*)a + (index + n) * item_size, (old_size - index - n) * item_size);
        array_size_lvalue(a) = old_size - n;
    }
}

static inline void array_move_impl(void** from, void** to)
{
    *to = *from;
    *from = NULL;
}

static inline int array_pointer_compare(const void* key, const void* element)
{
    if (*(void* const*)key > *(void* const*)element) return -1;
    else if (*(void* const*)key < *(void* const*)element) return 1;
    else return 0;
}

static inline int array_uint32_t_compare(const void* key, const void* element)
{
    if (*(const uint32_t*)key < *(const uint32_t*)element) return -1;
    else if (*(const uint32_t*)key == *(const uint32_t*)element) return 0;
    else return 1;
}

static inline int array_uint64_t_compare(const void* key, const void* element)
{
    if (*(const uint64_t*)key < *(const uint64_t*)element) return -1;
    else if (*(const uint64_t*)key == *(const uint64_t*)element) return 0;
    else return 1;
}

static inline void* array_find_impl(void* a, size_t item_size, int (*pred)(const void* key, const void* elem), const void* args)
{
    size_t len = array_size(a);
    for (size_t i = 0; i != len; i++)
    {
        void* e = (char*)a + i * item_size;
        if (pred && pred(args, e) == 0) return e;
    }
    return NULL;
}

static inline void array_compact_impl(void* a, size_t item_size, int (*pred)(void const* args, void const* elem), void const* args)
{
    void* slot = array_find_impl(a, item_size, pred, args);
    if (!slot) return;
    void* end = (char*)a + array_size(a) * item_size;
    if (slot != end)
    {
        for (void* i = (char*)slot + item_size; i != end; i = (char*)i + item_size)
        {
            if (pred(args, i) != 0)
            {
                memcpy(slot, i, (size_t)item_size);
                slot = (char*)slot + item_size;
            }
        }
    }
    if (a) array_size_lvalue(a) = ((char*)slot - (char*)a) / item_size;
}
