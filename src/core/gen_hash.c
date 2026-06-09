//$ #pragma once

#include "core/allocator.h"

#include "core/macros.h"
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

typedef struct Allocator Allocator;

// Load factor (%) at which the hash table should grow
#define HASH_GROW_THRESHOLD 77

// Load factor (%) at which the hash table should shrink
#define HASH_SHRINK_THRESHOLD 20

// Minimum number of buckets a hash table can have
#define HASH_MIN_BUCKETS 4

// Invalid index value
#define HASH_INVALID_INDEX UINT32_MAX

#define HASH_ALIGN_UP(val, align) (((val) + ((size_t)(align) - 1)) & ~((size_t)(align) - 1))

#define hash_size(h) ((h)->size)
#define hash_capacity(h) ((h)->num_buckets)
#define hash_key(h, i) (h)->keys[i]
#define hash_value(h, i) (h)->values[i]
#define hash_has_key(h, key) (hash_index(h, key) != UINT32_MAX)
#define hash_put(h, key, value)           \
    do                                    \
    {                                     \
        uint32_t i = hash_insert(h, key); \
        hash_value(h, i) = (value);       \
    } while (0)

#define hash_index_existed(h, i) ((h)->flags[i].is_occupied && !(h)->flags[i].is_deleted)
#define hash_key_existed(h, key) (hash_index(h, key) != HASH_INVALID_INDEX)

typedef union HashFlags
{
    struct
    {
        bool is_occupied : 1;
        bool is_deleted : 1;
    };

    uint8_t bits;
} HashFlags;

static inline uint32_t hash_index_next(uint32_t current, uint32_t d, uint32_t num_buckets)
{
    return (current + d) & (num_buckets - 1);
}

static inline bool hash_key_empty(HashFlags flags)
{
    return !flags.is_occupied;
}

static inline bool hash_is_grow_needed(uint32_t size, uint32_t num_buckets)
{
    return (uint64_t)size * 100 >= (uint64_t)num_buckets * HASH_GROW_THRESHOLD;
}

static inline uint32_t hash_round_up_to_power_of_two(uint32_t x)
{
    x = x - 1;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    x = x + 1;
    return x > HASH_MIN_BUCKETS ? x : HASH_MIN_BUCKETS;
}

/*$ remove */
typedef uint64_t $value_type$;
typedef uint64_t $key_type_raw$;
/*$*/

/*$ foreach keys -*/
/*$ if key_type_raw -*/
typedef $key_type_raw$ $key_type$;
/*$*/

static inline bool hash_key_equal_$key_type$($key_type$ key1, $key_type$ key2)
{
    /*$ switch key_type -*/
    /*$ case cstr_t -*/
    //$ return strcmp(key1, key2) == 0;
    /*$ case bytes_t -*/
    //$ if (key1.len != key2.len)
    //$ {
    //$     return false;
    //$ }
    //$ return memcmp(key1.data, key2.data, key1.len) == 0;
    /*$ default -*/
    return key1 == key2;
    /*$*/
}

static inline uint32_t hash_hash_func_$key_type$($key_type$ key, uint32_t num_buckets)
{
    /*$ switch key_type -*/
    /*$ case cstr_t -*/
    //$ uint32_t h = (uint32_t)(uint8_t)*key;
    //$ if (h)
    //$ {
    //$     for (++key; *key; ++key)
    //$     {
    //$         h = (h << 5) - h + (uint32_t)(uint8_t)*key;
    //$     }
    //$ }
    //$ return h & (num_buckets - 1);
    /*$ case uint64_t -*/
    return (key >> 33 ^ key ^ key << 11) & (num_buckets - 1);
    /*$ case ptr_t -*/
    //$ uint64_t key_u64 = (uint64_t)key;
    //$ return (key_u64 >> 33 ^ key_u64 ^ key_u64 << 11) & (num_buckets - 1);
    /*$ case bytes_t -*/
    //$ if (key.len == 0)
    //$ {
    //$     return 0;
    //$ }
    //$ int32_t h = (int32_t)key.data[0];
    //$ for (uint64_t i = 1; i != key.len; i++)
    //$ {
    //$     h = (h << 5) - h + (int32_t)key.data[i];
    //$ }
    //$ return h & (num_buckets - 1);
    /*$ default -*/
    //$ fatal("unreachable");
    /*$*/
}
/*$*/

/*$ remove */
#define $value_size$ sizeof($value_type$)
/*$*/

/*$ foreach hashs */
typedef struct $hash_name$
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    $key_type$* keys;
    /*$ if value_type */
    $value_type$* values;
    $value_type$ default_value;
    /*$*/
} $hash_name$;

static inline uint32_t hash_index_$hash_name$($hash_name$ const* h, $key_type$ key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_$key_type$(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_$key_type$(h->keys[i], key))
            {
                return i;
            }
            if (h->flags[i].is_deleted && b_get_deleted && deleted == HASH_INVALID_INDEX)
            {
                deleted = i;
            }
            i = hash_index_next(i, d + 1, h->num_buckets);
            if (i == begin)
            {
                return deleted;
            }
        }
        else
        {
            return b_get_deleted ? i : HASH_INVALID_INDEX;
        }
    }
    return HASH_INVALID_INDEX;
}

static inline uint32_t hash_index_no_check_$hash_name$(const HashFlags* flags, uint32_t num_buckets, $key_type$ key)
{
    expect(num_buckets, "num_buckets is zero");
    uint32_t begin = hash_hash_func_$key_type$(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        expect(i != begin, "index wrapped around");
    }
    return i;
}

static inline void hash_grow_$hash_name$($hash_name$* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof($key_type$);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    /*$ if value_type */
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof($value_type$));
    size_t default_value_offset = current_offset;
    current_offset += sizeof($value_type$);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof($value_type$);
    /*$*/

    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    expect(raw_mem, "allocation failed");

    $key_type$* new_keys = ($key_type$*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    /*$ if value_type */
    $value_type$* default_value = ($value_type$*)(raw_mem + default_value_offset);
    $value_type$* new_values = ($value_type$*)(raw_mem + values_offset);
    /*$*/

    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_$hash_name$(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        /*$ if value_type */
        new_values[index] = h->values[i];
        /*$*/
        if (index < lo)
        {
            lo = index;
        }
        if (index + 1 > hi)
        {
            hi = index + 1;
        }
    }
    allocator_free(h->allocator, h->keys);
    h->num_buckets = new_buckets;
    h->keys = new_keys;
    h->flags = new_flags;
    h->begin = lo == HASH_INVALID_INDEX ? 0 : lo;
    h->end = hi;
    /*$ if value_type */
    h->values = new_values;
    *default_value = h->default_value;
    /*$*/
}

static inline uint32_t hash_insert_check_$hash_name$($hash_name$* h, $key_type$ key, bool* b_existed)
{
    uint32_t index = hash_index_$hash_name$(h, key, true);
    if (index != HASH_INVALID_INDEX && hash_index_existed(h, index))
    {
        *b_existed = true;
        return index;
    }
    else
    {
        *b_existed = false;
    }
    h->size += 1;
    if (index == HASH_INVALID_INDEX || (hash_key_empty(h->flags[index]) && hash_is_grow_needed(h->size, h->num_buckets)))
    {
        hash_grow_$hash_name$(h, 1);
        index = hash_index_no_check_$hash_name$(h->flags, h->num_buckets, key);
    }
    if (h->begin == h->end)
    {
        h->begin = index;
    }
    else
    {
        if (index < h->begin)
        {
            h->begin = index;
        }
    }
    if (index + 1 > h->end)
    {
        h->end = index + 1;
    }
    h->keys[index] = key;
    /*$ if value_type -*/
    h->values[index] = h->default_value;
    /*$*/
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_$hash_name$($hash_name$* h, $key_type$ key)
{
    bool b_existed;
    return hash_insert_check_$hash_name$(h, key, &b_existed);
}

static inline void hash_remove_$hash_name$($hash_name$* h, uint32_t i)
{
    expect(h && h->size != 0, "hash is invalid or empty");
    expect(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false, "invalid hash entry state");
    h->flags[i].is_deleted = true;
    --h->size;
    if (h->begin == i)
    {
        h->begin = 0;
        for (uint32_t j = i; j != h->end; j++)
        {
            if (hash_index_existed(h, j))
            {
                h->begin = j;
                break;
            }
        }
    }
    if (h->end == i + 1)
    {
        h->end = 0;
        for (uint32_t j = i; j != HASH_INVALID_INDEX; j--)
        {
            if (hash_index_existed(h, j))
            {
                h->end = j + 1;
                break;
            }
        }
    }
}

static inline void hash_reset_$hash_name$($hash_name$* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_$hash_name$($hash_name$* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    /*$ if value_type -*/
    h->values = NULL;
    /*$*/
}

static inline uint32_t hash_next_$hash_name$($hash_name$* h, uint32_t start)
{
    for (uint32_t i = start + 1; i != h->end; i++)
    {
        if (hash_index_existed(h, i))
        {
            return i;
        }
    }
    return h->end;
}

/*$ if value_type -*/
static inline $value_type$ hash_get_$hash_name$($hash_name$ const* h, $key_type$ key)
{
    uint32_t index = hash_index_$hash_name$(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}
/*$ else */
#define hash_get_$hash_name$ NULL
/*$*/
/*$*/

#define hash_index(h, key)                          \
    _Generic(h, /*$ foreach hashs */                \
        $hash_name$*: hash_index_$hash_name$, /*$*/ \
        default: NULL)(h, key, false)

#define hash_insert(h, key)                          \
    _Generic(h, /*$ foreach hashs */                 \
        $hash_name$*: hash_insert_$hash_name$, /*$*/ \
        default: NULL)(h, key)

#define hash_insert_check(h, key, b)                       \
    _Generic(h, /*$ foreach hashs */                       \
        $hash_name$*: hash_insert_check_$hash_name$, /*$*/ \
        default: NULL)(h, key, b)

#define hash_remove(h, i)                            \
    _Generic(h, /*$ foreach hashs */                 \
        $hash_name$*: hash_remove_$hash_name$, /*$*/ \
        default: NULL)(h, i)

#define hash_free(h)                               \
    _Generic(h, /*$ foreach hashs */               \
        $hash_name$*: hash_free_$hash_name$, /*$*/ \
        default: NULL)(h)

#define hash_reset(h)                               \
    _Generic(h, /*$ foreach hashs */                \
        $hash_name$*: hash_reset_$hash_name$, /*$*/ \
        default: NULL)(h)

#define hash_next(h, key)                          \
    _Generic(h, /*$ foreach hashs */               \
        $hash_name$*: hash_next_$hash_name$, /*$*/ \
        default: NULL)(h, key)

#define hash_get(h, key)                          \
    _Generic(h, /*$ foreach hashs */              \
        $hash_name$*: hash_get_$hash_name$, /*$*/ \
        default: NULL)(h, key)

/*$*/

#include "core/template.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum key_type_e
{
    key_type_u64 = 0,
    key_type_ptr,
    key_type_cstr,
    key_type_bytes,
} key_type_e;

typedef enum value_type_e
{
    value_type_none = 0,
    value_type_u64,
    value_type_ptr,
    value_type_cstr,
} value_type_e;

struct hash_key_t
{
    char const* type;
    char const* key_raw;
} g_keys[] = {
    [key_type_u64] = {.type = "uint64_t", .key_raw = NULL},
    [key_type_ptr] = {.type = "ptr_t", .key_raw = "void*"},
    [key_type_cstr] = {.type = "cstr_t", .key_raw = "char const*"},
    [key_type_bytes] = {.type = "bytes_t", .key_raw = "struct {char* data; uint32_t len;}"},
};

struct hash_value_t
{
    char const* type;
    char const* size;
} g_values[] = {
    [value_type_none] = {
        .type = NULL,
        .size = "0",
    },
    [value_type_u64] = {
        .type = "uint64_t",
        .size = "sizeof(uint64_t)",
    },
    [value_type_ptr] = {
        .type = "void*",
        .size = "sizeof(void*)",
    },
    [value_type_cstr] = {
        .type = "char const*",
        .size = "sizeof(char const*)",
    },
};

struct hash_t
{
    char const* hash_name;
    key_type_e key_type;
    value_type_e value_type;
} g_hashs[] = {
    {
        .hash_name = "Set",
        .key_type = key_type_u64,
        .value_type = value_type_none,
    },
    {
        .hash_name = "Hash",
        .key_type = key_type_u64,
        .value_type = value_type_u64,
    },
    {
        .hash_name = "PtrHash",
        .key_type = key_type_ptr,
        .value_type = value_type_ptr,
    },
    {
        .hash_name = "StringHash",
        .key_type = key_type_cstr,
        .value_type = value_type_u64,
    },
    {
        .hash_name = "StringPtrHash",
        .key_type = key_type_cstr,
        .value_type = value_type_ptr,
    },
    {
        .hash_name = "StringSet",
        .key_type = key_type_cstr,
        .value_type = value_type_none,
    },
};

static void add_keys(template_t* tpl)
{
    value_t key_array;
    template_array_init(&key_array, tpl->allocator);
    for (uint64_t i = 0; i != sizeof(g_keys) / sizeof(*g_keys); i++)
    {
        struct hash_key_t* k = &g_keys[i];
        value_t dict;
        template_dict_init(&dict, tpl->allocator);
        value_t key_type = {.type = template_value_string, .str = k->type};
        template_dict_add(&dict.dict, "key_type", &key_type);
        value_t key_type_raw = {.type = template_value_string, .str = k->key_raw};
        template_dict_add(&dict.dict, "key_type_raw", &key_type_raw);
        template_array_add(&key_array.array, &dict);
    }
    template_dict_add(&tpl->value.dict, "keys", &key_array);
}

static void add_values(template_t* tpl)
{
    value_t value_array;
    template_array_init(&value_array, tpl->allocator);
    for (uint64_t i = 0; i != sizeof(g_values) / sizeof(*g_values); i++)
    {
        struct hash_value_t* v = &g_values[i];
        value_t dict;
        template_dict_init(&dict, tpl->allocator);
        value_t value_type = {.type = template_value_string, .str = v->type};
        template_dict_add(&dict.dict, "value_type", &value_type);
        value_t value_size = {.type = template_value_string, .str = v->size};
        template_dict_add(&dict.dict, "value_size", &value_size);
        template_array_add(&value_array.array, &dict);
    }
    template_dict_add(&tpl->value.dict, "values", &value_array);
}

static void add_hashs(template_t* tpl)
{
    value_t hash_array;
    template_array_init(&hash_array, tpl->allocator);
    for (uint64_t i = 0; i != sizeof(g_hashs) / sizeof(*g_hashs); i++)
    {
        struct hash_t const* h = &g_hashs[i];
        value_t dict;
        template_dict_init(&dict, tpl->allocator);
        value_t hash_name = {.type = template_value_string, .str = h->hash_name};
        template_dict_add(&dict.dict, "hash_name", &hash_name);
        value_t key_type = {.type = template_value_string, .str = g_keys[h->key_type].type};
        template_dict_add(&dict.dict, "key_type", &key_type);
        value_t value_type = {.type = template_value_string, .str = g_values[h->value_type].type};
        template_dict_add(&dict.dict, "value_type", &value_type);
        value_t value_size = {.type = template_value_string, .str = g_values[h->value_type].size};
        template_dict_add(&dict.dict, "value_size", &value_size);
        template_array_add(&hash_array.array, &dict);
    }
    template_dict_add(&tpl->value.dict, "hashs", &hash_array);
}

void* AllocatorC_malloc(Allocator* instance, size_t size)
{
    return malloc(size);
}

void* AllocatorC_calloc(Allocator* instance, size_t count, size_t size)
{
    return calloc(count, size);
}

void* AllocatorC_realloc(Allocator* instance, void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void AllocatorC_free(Allocator* instance, void* ptr)
{
    free(ptr);
}

void AllocatorC_destroy(Allocator* instance)
{
}

Allocator AllocatorC = {
    .malloc = AllocatorC_malloc,
    .calloc = AllocatorC_calloc,
    .realloc = AllocatorC_realloc,
    .free = AllocatorC_free,
    .destroy = AllocatorC_destroy,
};

int main(int argc, char** argv)
{
    char const* input = NULL;
    char const* output = NULL;
    for (int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if (strcmp(arg, "-o") == 0)
        {
            i += 1;
            output = argv[i];
            if (!output || output[0] == '-') break;
        }
        else if (arg[0] != '-') input = arg;
    }
    if (input == NULL || output == NULL)
    {
        printf("Usage: gen_hash -o <output> <input>\n");
        return EXIT_FAILURE;
    }
    FILE* input_file = fopen(input, "rb");
    expect(input_file, "input file is NULL");
    FILE* output_file = fopen(output, "wb");
    expect(output_file, "output file is NULL");
    template_t* tpl = template_create(&AllocatorC);
    add_keys(tpl);
    add_values(tpl);
    add_hashs(tpl);
    template_gen(tpl, input_file, output_file);
    fclose(input_file);
    fclose(output_file);
    template_destroy(tpl);
}
