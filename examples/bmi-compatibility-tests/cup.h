#ifndef CUP_H

#define COMPILER_CLANG 1
#define COMPILER_GCC 2
#define COMPILER_CL 3
#define COMPILER_TCC 4

#if defined(__clang__) && !defined(__CLANGD__)
#define CURRENT_COMPILER COMPILER_CLANG
#elif defined(__TINYC__)
#define CURRENT_COMPILER COMPILER_TCC
#elif defined(__GNUC__)
#define CURRENT_COMPILER COMPILER_GCC
#elif defined(_MSC_VER)
#define CURRENT_COMPILER COMPILER_CL
#endif

#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2
#define PLATFORM_MACOS 3
#define PLATFORM_ANDROID 4
#define PLATFORM_IOS 5

#ifdef _WIN32
#define CURRENT_PLATFORM PLATFORM_WINDOWS
#elif __APPLE__
#define CURRENT_PLATFORM PLATFORM_MACOS
#elif __linux__
#define CURRENT_PLATFORM PLATFORM_LINUX
#else
#error "Unknown platform"
#endif

#define ARCHITECTURE_X86 1
#define ARCHITECTURE_X64 2
#define ARCHITECTURE_ARM7 3
#define ARCHITECTURE_ARM8 4
#define ARCHITECTURE_UNKNOW 64

#ifdef _MSC_VER
#ifdef _M_X64
#define CURRENT_ARCHITECTURE ARCHITECTURE_X64
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X86
#else
#define CURRENT_ARCHITECTURE ARCHITECTURE_UNKNOW
#endif
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
#if defined(__i386__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X86
#elif defined(__x86_64__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_X64
#elif defined(__aarch64__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_ARM8
#elif defined(__arm__)
#define CURRENT_ARCHITECTURE ARCHITECTURE_ARM7
#else
#error "Unknown architecture for GCC/Clang"
#endif
#else
#error "Unknown compiler, unable to determine architecture"
#endif

#ifndef CONSTRUCTOR
#if CURRENT_COMPILER == COMPILER_CL
#define CONSTRUCTOR(name)                   \
    _Pragma("section(\".CRT$XCU\", read)"); \
    void name(void);                        \
    __declspec(allocate(".CRT$XCU")) void (*name##_ptr)(void) = name;
#define DESTRUCTOR(name)                    \
    _Pragma("section(\".CRT$XTU\", read)"); \
    void name(void);                        \
    __declspec(allocate(".CRT$XTU")) void (*name##_ptr)(void) = name;
#else
#define CONSTRUCTOR(name) __attribute__((constructor(101)))
#define DESTRUCTOR(name) __attribute__((destructor(101)))
#endif
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#else
#define DLL_EXPORT __attribute__((visibility("default")))
#define DLL_IMPORT
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#define EXE_EXT ".exe"
#define DLL_EXT ".dll"
#define OBJ_EXT ".obj"
#define LIB_EXT ".lib"
#else
#define EXE_EXT ""
#define DLL_EXT ".so"
#define OBJ_EXT ".o"
#define LIB_EXT ".a"
#endif

static inline char const* get_platform_string(int type)
{
    switch (type)
    {
    case PLATFORM_WINDOWS: return "windows";
    case PLATFORM_LINUX: return "linux";
    case PLATFORM_MACOS: return "mac";
    case PLATFORM_ANDROID: return "android";
    case PLATFORM_IOS: return "ios";
    default: return "unknown platform";
    }
}

#if CURRENT_COMPILER == COMPILER_CLANG
#pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"
#endif

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

#if defined(__APPLE__) || defined(__STDC_NO_THREADS__) || \
    (defined(__has_include) && !__has_include(<threads.h>))

#include <errno.h>
#include <pthread.h>

typedef pthread_mutex_t mtx_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;
#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
enum
{
    mtx_plain = 0,
    mtx_recursive = 1,
    mtx_timed = 2
};

#define thrd_success 0
#define thrd_busy 1
#define thrd_error -1
static inline int mtx_init(mtx_t* mtx, int type)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    if (type & mtx_recursive)
    {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }

    int r = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return r == 0 ? thrd_success : thrd_error;
}

static inline void mtx_destroy(mtx_t* mtx)
{
    pthread_mutex_destroy(mtx);
}

static inline int mtx_lock(mtx_t* mtx)
{
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_trylock(mtx_t* mtx)
{
    int r = pthread_mutex_trylock(mtx);
    if (r == 0) return thrd_success;
    if (r == EBUSY) return thrd_busy;
    return thrd_error;
}

static inline int mtx_unlock(mtx_t* mtx)
{
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int tss_create(tss_t* key, void (*dtor)(void*))
{
    return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

static inline void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int tss_set(tss_t key, void* val)
{
    return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}

static inline void tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

static inline void call_once(once_flag* flag, void (*func)(void))
{
    pthread_once(flag, func);
}

#else
#include <threads.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Allocator Allocator;
typedef struct Process Process;
typedef struct LockFileContext
{
    FILE* file;
} LockFileContext;

uint64_t os_get_mtime(char const* path);
bool os_file_exists(char const* path);
char* os_get_env(Allocator* allocator, const char* name);
void os_set_env(char const* name, char const* env);
wchar_t* os_get_env_block(Allocator* allocator);
wchar_t* os_get_default_env(void);
void os_reset_env(void);
char* os_get_cwd(Allocator* allocator);
bool os_set_cwd(char const* path);
void os_ensure_dir_existed(char const* path);
char const* os_get_cmdline(void);
bool os_remove_file(char const* path);
char* os_get_current_exe_path(Allocator* allocator);
bool os_rename(char const* old_path, char const* new_path);
bool os_copy_file(char const* src, char const* dst);
bool os_mkdir(char const* path);
bool os_create_directory_tree(char const* path);
char* os_create_guid(Allocator* allocator, bool lowercase);
uint64_t os_get_rand_uint64(void);
int os_get_cpu_count(void);
FILE* os_fopen(char const* path, char const* mode);
FILE* os_popen(char const* cmd, char const* mode);
int os_pclose(FILE* file);
Process* os_start_process(char const* cmd);
int os_wait_process(Process* p);
void os_forget_process(Process* p);
uint64_t os_get_file_size(char const* path);
char* os_read_all(Allocator* allocator, char const* path);
bool os_write_all(char const* path, char const* content, size_t size);
char* os_full_path(char const* path, Allocator* allocator);
LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared);
bool os_unlock_file(LockFileContext* context);
int os_ftruncate(FILE* f, long size);
bool os_file_writable(const char* path);
bool os_is_terminal_supports_color(void);
void os_set_console_utf8(void);

#include <stddef.h>
#include <stdio.h>

#define field_parent(type, ptr, field) (type*)((uint8_t*)(ptr) - offsetof(type, field))
#define stringify(token) #token
#define static_array_size(a) (sizeof(a) / sizeof((a)[0]))
#define error(fmt, ...) fprintf(stderr, "error: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define warn(fmt, ...) fprintf(stderr, "warning: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define ENUM_CODE(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

typedef struct StringHash StringHash;
typedef struct Allocator Allocator;

typedef enum JsonType
{
    JSON_TYPE_INVALID,
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_TRUE,
    JSON_TYPE_FALSE,
    JSON_TYPE_NULL,
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonObject JsonObject;
typedef JsonValue* JsonArray;
typedef char* JsonString;
typedef double JsonNumber;

struct JsonObject
{
    StringHash* hash_name_to_index;
    char const** keys;
    JsonValue* values;
};

struct JsonValue
{
    JsonType type;
    union
    {
        JsonObject object;
        JsonArray array;
        JsonString string;
        JsonNumber number;
    };
};

JsonValue json_from_string(char const* str, Allocator* allocator);
JsonValue* json_object_get_value(JsonObject* object, char const* key);


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


#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

static inline int utf8_decode(uint8_t const* p, uint32_t* out)
{
    uint8_t a = *p;
    if (a < 0x80)
    {
        *out = a;
        return 1;
    }
    else if ((a & 0xE0) == 0xC0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80)
        {
            return 0;
        }
        *out = (a & 0x1F) << 6 | (b & 0x3F);
        return 2;
    }
    else if ((a & 0xF0) == 0xE0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80) return 0;
        uint8_t c = *(p + 2);
        if ((c & 0xC0) != 0x80) return 0;
        *out = ((a & 0xF) << 12) | ((b & 0x3F) << 6) | (c & 0x3F);
        return 3;
    }
    else if ((a & 0xF8) == 0xF0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80) return 0;
        uint8_t c = *(p + 2);
        if ((c & 0xC0) != 0x80) return 0;
        uint8_t d = *(p + 3);
        if ((d & 0xC0) != 0x80) return 0;
        *out = ((a & 0x07) << 18) | ((b & 0x3F) << 12) | ((c & 0x3F) << 6) | (d & 0x3F);
        return 4;
    }
    return 0;
}

static inline int utf8_encode(uint32_t u, char out[4])
{
    if (u <= 0x7F)
    {
        out[0] = (char)u;
        return 1;
    }
    else if (u <= 0x7FF)
    {
        out[0] = (char)(0xC0 | (u >> 6));
        out[1] = (char)(0x80 | (u & 0x3F));
        return 2;
    }
    else if (u <= 0xFFFF)
    {
        out[0] = (char)(0xE0 | (u >> 12));
        out[1] = (char)(0x80 | ((u >> 6) & 0x3F));
        out[2] = (char)(0x80 | (u & 0x3F));
        return 3;
    }
    else
    {
        out[0] = (char)(0xF0 | (u >> 18));
        out[1] = (char)(0x80 | ((u >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((u >> 6) & 0x3F));
        out[3] = (char)(0x80 | (u & 0x3F));
        return 4;
    }
}

static inline int unicode_to_wchars(uint32_t u, uint16_t out[2])
{
    if (u <= 0xFFFF)
    {
        out[0] = (uint16_t)u;
        return 1;
    }
    else
    {
        uint32_t cp_offset = u - 0x10000;
        out[0] = (uint16_t)(0xD800 | (cp_offset >> 10));
        out[1] = (uint16_t)(0xDC00 | (cp_offset & 0x03FF));
        return 2;
    }
}

static inline int wchars_to_unicode(wchar_t const* p, uint32_t* out)
{
    uint32_t u = *p;
    if (u >= 0xD800 && u <= 0xD8FF)
    {
        uint32_t high = u;
        uint32_t low = *(p + 1);
        if (low >= 0xDC00 && low <= 0xDFFF)
        {
            u = 0x10000 + (((high & 0x3FF) << 10) | (low & 0x03FF));
            *out = u;
            return 2;
        }
        else
        {
            return 0;
        }
    }
    else if (u >= 0xDC00 && u <= 0xDFFF)
    {
        return 0;
    }
    *out = u;
    return 1;
}

static inline wchar_t* utf8_to_wchars(Allocator* allocator, const char* mbs)
{
    wchar_t* result = NULL;
    uint8_t const* p = (uint8_t const*)mbs;
    uint32_t u;
    while (1)
    {
        int n = utf8_decode(p, &u);
        if (n == 0) goto Error;
        p += n;
        uint16_t wchars[2];
        n = unicode_to_wchars(u, wchars);
        array_push_v(allocator, result, wchars, n);
        if (n == 1 && wchars[0] == 0)
        {
            break;
        }
    }
    array_push(allocator, result, L'\0');
    array_pop(result);
    return result;
Error:
    if (result)
    {
        array_free(allocator, result);
    }
    return NULL;
}

static inline char* wchars_to_utf8(Allocator* allocator, wchar_t const* wchars)
{
    char* result = NULL;
    wchar_t const* p = wchars;
    while (1)
    {
        uint32_t u;
        int n = wchars_to_unicode(p, &u);
        if (n == 0) goto Error;
        p += n;
        char out[4];
        n = utf8_encode(u, out);
        array_push_v(allocator, result, out, n);
        if (out[0] == 0)
        {
            break;
        }
    }
    array_pop(result);
    return result;
Error:
    if (result)
    {
        array_free(allocator, result);
    }
    return NULL;
}

static inline bool utf16_is_surrogate(uint16_t code)
{
    return code >= 0xD800 && code <= 0xDFFF;
}

static inline int utf16_to_utf32(const uint16_t* code, uint32_t* out)
{
    if (utf16_is_surrogate(*code))
    {
        if ((code[0] & 0xFFFFFC00) != 0xD800) return 0;
        if ((code[1] & 0xFFFFFC00) != 0xDC00) return 0;
        *out = ((uint32_t)code[0] << 10) + code[1] - 0x35FDC00;
        return 2;
    }
    *out = (uint32_t)*code;
    return 1;
}

static inline size_t base64_encode_size(size_t data_len)
{
    return ((data_len + 2) / 3) * 4 + 1;
}

static inline void base64_encode(char* out, uint8_t const* data, size_t data_len)
{
    // clang-format off
    static char const table[64] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
    };
    // clang-format on

    size_t fast_len = data_len - (data_len % 3);
    for (size_t i = 0; i < fast_len; i += 3)
    {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = table[(v >> 6) & 0x3F];
        *out++ = table[v & 0x3F];
    }

    size_t rem = data_len - fast_len;
    if (rem == 1)
    {
        uint32_t v = (uint32_t)data[fast_len] << 16;
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = '=';
        *out++ = '=';
    }
    else if (rem == 2)
    {
        uint32_t v = ((uint32_t)data[fast_len] << 16) | ((uint32_t)data[fast_len + 1] << 8);
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = table[(v >> 6) & 0x3F];
        *out++ = '=';
    }
    *out = '\0';
}

static inline int base64_decode(char const* in, size_t in_len, uint8_t* out, size_t out_cap)
{
    // clang-format off
    static uint8_t const decode_table[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,62, 255,255,255,63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255,255,255,255,255,255,
        255,0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255,255,255,255,255,
        255,26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255,255,255,255,255,
    };
    // clang-format on

    while (in_len > 0 && in[in_len - 1] == '=')
    {
        in_len--;
    }
    if (in_len == 0) return 0;

    size_t rem = in_len % 4;
    if (rem == 1) return -1;

    size_t out_len = (in_len / 4) * 3 + (rem == 2 ? 1 : (rem == 3 ? 2 : 0));
    if (out_len > out_cap) return -1;

    size_t fast_iters = in_len / 4;
    size_t in_pos = 0;
    size_t pos = 0;

    for (size_t i = 0; i < fast_iters; i++)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c2 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c3 = decode_table[(uint8_t)in[in_pos++]];

        if ((c0 | c1 | c2 | c3) & 0xC0) return -1;

        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        out[pos++] = (uint8_t)((c1 << 4) | (c2 >> 2));
        out[pos++] = (uint8_t)((c2 << 6) | c3);
    }

    if (rem == 2)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos]];
        if ((c0 | c1) & 0xC0) return -1;
        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
    }
    else if (rem == 3)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c2 = decode_table[(uint8_t)in[in_pos]];
        if ((c0 | c1 | c2) & 0xC0) return -1;
        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        out[pos++] = (uint8_t)((c1 << 4) | (c2 >> 2));
    }

    return (int)out_len;
}


#include <assert.h>
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

static inline bool hash_key_equal_uint64_t(uint64_t key1, uint64_t key2)
{
    return key1 == key2;
}

static inline uint32_t hash_hash_func_uint64_t(uint64_t key, uint32_t num_buckets)
{
    return (key >> 33 ^ key ^ key << 11) & (num_buckets - 1);
}
typedef char const* cstr_t;
static inline bool hash_key_equal_cstr_t(cstr_t key1, cstr_t key2)
{
    return strcmp(key1, key2) == 0;
}

static inline uint32_t hash_hash_func_cstr_t(cstr_t key, uint32_t num_buckets)
{
    uint32_t h = (uint32_t)(uint8_t)*key;
    if (h)
    {
        for (++key; *key; ++key)
        {
            h = (h << 5) - h + (uint32_t)(uint8_t)*key;
        }
    }
    return h & (num_buckets - 1);
}
typedef struct {char* data; uint32_t len;} bytes_t;
static inline bool hash_key_equal_bytes_t(bytes_t key1, bytes_t key2)
{
    if (key1.len != key2.len)
    {
        return false;
    }
    return memcmp(key1.data, key2.data, key1.len) == 0;
}

static inline uint32_t hash_hash_func_bytes_t(bytes_t key, uint32_t num_buckets)
{
    if (key.len == 0)
    {
        return 0;
    }
    int32_t h = (int32_t)key.data[0];
    for (uint64_t i = 1; i != key.len; i++)
    {
        h = (h << 5) - h + (int32_t)key.data[i];
    }
    return h & (num_buckets - 1);
}

typedef struct Set
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    uint64_t* keys;
} Set;

static inline uint32_t hash_index_Set(Set const* h, uint64_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_uint64_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_uint64_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_Set(const HashFlags* flags, uint32_t num_buckets, uint64_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_uint64_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_Set(Set* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(uint64_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    uint64_t* new_keys = (uint64_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_Set(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
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
}

static inline uint32_t hash_insert_check_Set(Set* h, uint64_t key, bool* b_existed)
{
    uint32_t index = hash_index_Set(h, key, true);
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
        hash_grow_Set(h, 1);
        index = hash_index_no_check_Set(h->flags, h->num_buckets, key);
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
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_Set(Set* h, uint64_t key)
{
    bool b_existed;
    return hash_insert_check_Set(h, key, &b_existed);
}

static inline void hash_remove_Set(Set* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_Set(Set* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_Set(Set* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
}

static inline uint32_t hash_next_Set(Set* h, uint32_t start)
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


#define hash_get_Set NULL

typedef struct Hash
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    uint64_t* keys;
    
    uint64_t* values;
    uint64_t default_value;
} Hash;

static inline uint32_t hash_index_Hash(Hash const* h, uint64_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_uint64_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_uint64_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_Hash(const HashFlags* flags, uint32_t num_buckets, uint64_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_uint64_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_Hash(Hash* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(uint64_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(uint64_t));
    size_t default_value_offset = current_offset;
    current_offset += sizeof(uint64_t);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof(uint64_t);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    uint64_t* new_keys = (uint64_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    
    uint64_t* default_value = (uint64_t*)(raw_mem + default_value_offset);
    uint64_t* new_values = (uint64_t*)(raw_mem + values_offset);
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_Hash(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        
        new_values[index] = h->values[i];
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
    
    h->values = new_values;
    *default_value = h->default_value;
}

static inline uint32_t hash_insert_check_Hash(Hash* h, uint64_t key, bool* b_existed)
{
    uint32_t index = hash_index_Hash(h, key, true);
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
        hash_grow_Hash(h, 1);
        index = hash_index_no_check_Hash(h->flags, h->num_buckets, key);
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
    h->values[index] = h->default_value;
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_Hash(Hash* h, uint64_t key)
{
    bool b_existed;
    return hash_insert_check_Hash(h, key, &b_existed);
}

static inline void hash_remove_Hash(Hash* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_Hash(Hash* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_Hash(Hash* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    h->values = NULL;
}

static inline uint32_t hash_next_Hash(Hash* h, uint32_t start)
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

static inline uint64_t hash_get_Hash(Hash const* h, uint64_t key)
{
    uint32_t index = hash_index_Hash(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}

typedef struct StringHash
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    cstr_t* keys;
    
    uint64_t* values;
    uint64_t default_value;
} StringHash;

static inline uint32_t hash_index_StringHash(StringHash const* h, cstr_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_cstr_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_cstr_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_StringHash(const HashFlags* flags, uint32_t num_buckets, cstr_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_cstr_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_StringHash(StringHash* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(cstr_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(uint64_t));
    size_t default_value_offset = current_offset;
    current_offset += sizeof(uint64_t);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof(uint64_t);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    cstr_t* new_keys = (cstr_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    
    uint64_t* default_value = (uint64_t*)(raw_mem + default_value_offset);
    uint64_t* new_values = (uint64_t*)(raw_mem + values_offset);
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_StringHash(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        
        new_values[index] = h->values[i];
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
    
    h->values = new_values;
    *default_value = h->default_value;
}

static inline uint32_t hash_insert_check_StringHash(StringHash* h, cstr_t key, bool* b_existed)
{
    uint32_t index = hash_index_StringHash(h, key, true);
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
        hash_grow_StringHash(h, 1);
        index = hash_index_no_check_StringHash(h->flags, h->num_buckets, key);
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
    h->values[index] = h->default_value;
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_StringHash(StringHash* h, cstr_t key)
{
    bool b_existed;
    return hash_insert_check_StringHash(h, key, &b_existed);
}

static inline void hash_remove_StringHash(StringHash* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_StringHash(StringHash* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_StringHash(StringHash* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    h->values = NULL;
}

static inline uint32_t hash_next_StringHash(StringHash* h, uint32_t start)
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

static inline uint64_t hash_get_StringHash(StringHash const* h, cstr_t key)
{
    uint32_t index = hash_index_StringHash(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}

typedef struct BytesHash
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    bytes_t* keys;
    
    uint64_t* values;
    uint64_t default_value;
} BytesHash;

static inline uint32_t hash_index_BytesHash(BytesHash const* h, bytes_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_bytes_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_bytes_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_BytesHash(const HashFlags* flags, uint32_t num_buckets, bytes_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_bytes_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_BytesHash(BytesHash* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(bytes_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(uint64_t));
    size_t default_value_offset = current_offset;
    current_offset += sizeof(uint64_t);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof(uint64_t);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    bytes_t* new_keys = (bytes_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    
    uint64_t* default_value = (uint64_t*)(raw_mem + default_value_offset);
    uint64_t* new_values = (uint64_t*)(raw_mem + values_offset);
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_BytesHash(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        
        new_values[index] = h->values[i];
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
    
    h->values = new_values;
    *default_value = h->default_value;
}

static inline uint32_t hash_insert_check_BytesHash(BytesHash* h, bytes_t key, bool* b_existed)
{
    uint32_t index = hash_index_BytesHash(h, key, true);
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
        hash_grow_BytesHash(h, 1);
        index = hash_index_no_check_BytesHash(h->flags, h->num_buckets, key);
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
    h->values[index] = h->default_value;
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_BytesHash(BytesHash* h, bytes_t key)
{
    bool b_existed;
    return hash_insert_check_BytesHash(h, key, &b_existed);
}

static inline void hash_remove_BytesHash(BytesHash* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_BytesHash(BytesHash* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_BytesHash(BytesHash* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    h->values = NULL;
}

static inline uint32_t hash_next_BytesHash(BytesHash* h, uint32_t start)
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

static inline uint64_t hash_get_BytesHash(BytesHash const* h, bytes_t key)
{
    uint32_t index = hash_index_BytesHash(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}

typedef struct StringPtrHash
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    cstr_t* keys;
    
    void** values;
    void* default_value;
} StringPtrHash;

static inline uint32_t hash_index_StringPtrHash(StringPtrHash const* h, cstr_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_cstr_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_cstr_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_StringPtrHash(const HashFlags* flags, uint32_t num_buckets, cstr_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_cstr_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_StringPtrHash(StringPtrHash* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(cstr_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(void*));
    size_t default_value_offset = current_offset;
    current_offset += sizeof(void*);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof(void*);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    cstr_t* new_keys = (cstr_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    
    void** default_value = (void**)(raw_mem + default_value_offset);
    void** new_values = (void**)(raw_mem + values_offset);
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_StringPtrHash(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        
        new_values[index] = h->values[i];
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
    
    h->values = new_values;
    *default_value = h->default_value;
}

static inline uint32_t hash_insert_check_StringPtrHash(StringPtrHash* h, cstr_t key, bool* b_existed)
{
    uint32_t index = hash_index_StringPtrHash(h, key, true);
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
        hash_grow_StringPtrHash(h, 1);
        index = hash_index_no_check_StringPtrHash(h->flags, h->num_buckets, key);
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
    h->values[index] = h->default_value;
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_StringPtrHash(StringPtrHash* h, cstr_t key)
{
    bool b_existed;
    return hash_insert_check_StringPtrHash(h, key, &b_existed);
}

static inline void hash_remove_StringPtrHash(StringPtrHash* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_StringPtrHash(StringPtrHash* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_StringPtrHash(StringPtrHash* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    h->values = NULL;
}

static inline uint32_t hash_next_StringPtrHash(StringPtrHash* h, uint32_t start)
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

static inline void* hash_get_StringPtrHash(StringPtrHash const* h, cstr_t key)
{
    uint32_t index = hash_index_StringPtrHash(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}

typedef struct StringSet
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    cstr_t* keys;
} StringSet;

static inline uint32_t hash_index_StringSet(StringSet const* h, cstr_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_cstr_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_cstr_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_StringSet(const HashFlags* flags, uint32_t num_buckets, cstr_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_cstr_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_StringSet(StringSet* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(cstr_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    cstr_t* new_keys = (cstr_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_StringSet(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
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
}

static inline uint32_t hash_insert_check_StringSet(StringSet* h, cstr_t key, bool* b_existed)
{
    uint32_t index = hash_index_StringSet(h, key, true);
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
        hash_grow_StringSet(h, 1);
        index = hash_index_no_check_StringSet(h->flags, h->num_buckets, key);
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
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_StringSet(StringSet* h, cstr_t key)
{
    bool b_existed;
    return hash_insert_check_StringSet(h, key, &b_existed);
}

static inline void hash_remove_StringSet(StringSet* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_StringSet(StringSet* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_StringSet(StringSet* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
}

static inline uint32_t hash_next_StringSet(StringSet* h, uint32_t start)
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


#define hash_get_StringSet NULL

typedef struct StringStringHash
{
    Allocator* allocator;
    uint32_t num_buckets;
    uint32_t size;
    HashFlags* flags;
    uint32_t begin;
    uint32_t end;
    cstr_t* keys;
    
    char const** values;
    char const* default_value;
} StringStringHash;

static inline uint32_t hash_index_StringStringHash(StringStringHash const* h, cstr_t key, bool b_get_deleted)
{
    if (h->num_buckets == 0)
    {
        return HASH_INVALID_INDEX;
    }
    uint32_t begin = hash_hash_func_cstr_t(key, h->num_buckets);
    uint32_t deleted = HASH_INVALID_INDEX;
    for (uint32_t d = 0, i = begin;; ++d)
    {
        if (h->flags[i].is_occupied)
        {
            if (!h->flags[i].is_deleted && hash_key_equal_cstr_t(h->keys[i], key))
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

static inline uint32_t hash_index_no_check_StringStringHash(const HashFlags* flags, uint32_t num_buckets, cstr_t key)
{
    assert(num_buckets);
    uint32_t begin = hash_hash_func_cstr_t(key, num_buckets);
    uint32_t i = begin;
    uint32_t d = 0;
    while (flags[i].is_occupied)
    {
        i = hash_index_next(i, ++d, num_buckets);
        assert(i != begin);
    }
    return i;
}

static inline void hash_grow_StringStringHash(StringStringHash* h, uint32_t grow_size)
{
    uint32_t new_buckets = hash_round_up_to_power_of_two(grow_size + h->num_buckets);

    size_t current_offset = 0;
    // Keys
    size_t keys_offset = current_offset;
    current_offset += new_buckets * sizeof(cstr_t);

    // Flags
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(HashFlags));
    size_t flags_offset = current_offset;
    current_offset += new_buckets * sizeof(HashFlags);
    
    // Values
    current_offset = HASH_ALIGN_UP(current_offset, _Alignof(char const*));
    size_t default_value_offset = current_offset;
    current_offset += sizeof(char const*);
    size_t values_offset = current_offset;
    current_offset += new_buckets * sizeof(char const*);
    size_t total_bytes = current_offset;

    char* raw_mem = allocator_malloc(h->allocator, total_bytes);
    assert(raw_mem);

    cstr_t* new_keys = (cstr_t*)(raw_mem + keys_offset);
    HashFlags* new_flags = (HashFlags*)(raw_mem + flags_offset);
    memset(new_flags, 0, new_buckets * sizeof(HashFlags));
    
    char const** default_value = (char const**)(raw_mem + default_value_offset);
    char const** new_values = (char const**)(raw_mem + values_offset);
    uint32_t lo = HASH_INVALID_INDEX;
    uint32_t hi = 0;
    for (uint64_t i = 0; i < h->num_buckets; i++)
    {
        if (!hash_index_existed(h, i))
        {
            continue;
        }
        uint32_t index = hash_index_no_check_StringStringHash(new_flags, new_buckets, h->keys[i]);
        new_flags[index].is_deleted = false;
        new_flags[index].is_occupied = true;
        new_keys[index] = h->keys[i];
        
        new_values[index] = h->values[i];
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
    
    h->values = new_values;
    *default_value = h->default_value;
}

static inline uint32_t hash_insert_check_StringStringHash(StringStringHash* h, cstr_t key, bool* b_existed)
{
    uint32_t index = hash_index_StringStringHash(h, key, true);
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
        hash_grow_StringStringHash(h, 1);
        index = hash_index_no_check_StringStringHash(h->flags, h->num_buckets, key);
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
    h->values[index] = h->default_value;
    h->flags[index].is_deleted = false;
    h->flags[index].is_occupied = true;
    return index;
}

static inline uint32_t hash_insert_StringStringHash(StringStringHash* h, cstr_t key)
{
    bool b_existed;
    return hash_insert_check_StringStringHash(h, key, &b_existed);
}

static inline void hash_remove_StringStringHash(StringStringHash* h, uint32_t i)
{
    assert(h && h->size != 0);
    assert(h->flags[i].is_occupied == true && h->flags[i].is_deleted == false);
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

static inline void hash_reset_StringStringHash(StringStringHash* h)
{
    if (h->flags)
    {
        memset(h->flags, 0, h->num_buckets * sizeof(HashFlags));
    }
    h->size = 0;
    h->begin = 0;
    h->end = 0;
}

static inline void hash_free_StringStringHash(StringStringHash* h)
{
    allocator_free(h->allocator, h->keys);
    h->num_buckets = 0;
    h->size = 0;
    h->flags = NULL;
    h->begin = 0;
    h->end = 0;
    h->keys = NULL;
    h->values = NULL;
}

static inline uint32_t hash_next_StringStringHash(StringStringHash* h, uint32_t start)
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

static inline char const* hash_get_StringStringHash(StringStringHash const* h, cstr_t key)
{
    uint32_t index = hash_index_StringStringHash(h, key, false);
    if (index == HASH_INVALID_INDEX)
    {
        return h->default_value;
    }
    else
    {
        return h->values[index];
    }
}
#define hash_index(h, key)                          \
    _Generic(h,                 \
        Set*: hash_index_Set,                 \
        Hash*: hash_index_Hash,                 \
        StringHash*: hash_index_StringHash,                 \
        BytesHash*: hash_index_BytesHash,                 \
        StringPtrHash*: hash_index_StringPtrHash,                 \
        StringSet*: hash_index_StringSet,                 \
        StringStringHash*: hash_index_StringStringHash,  \
        default: NULL)(h, key, false)

#define hash_insert(h, key)                          \
    _Generic(h,                  \
        Set*: hash_insert_Set,                  \
        Hash*: hash_insert_Hash,                  \
        StringHash*: hash_insert_StringHash,                  \
        BytesHash*: hash_insert_BytesHash,                  \
        StringPtrHash*: hash_insert_StringPtrHash,                  \
        StringSet*: hash_insert_StringSet,                  \
        StringStringHash*: hash_insert_StringStringHash,  \
        default: NULL)(h, key)

#define hash_insert_check(h, key, b)                       \
    _Generic(h,                        \
        Set*: hash_insert_check_Set,                        \
        Hash*: hash_insert_check_Hash,                        \
        StringHash*: hash_insert_check_StringHash,                        \
        BytesHash*: hash_insert_check_BytesHash,                        \
        StringPtrHash*: hash_insert_check_StringPtrHash,                        \
        StringSet*: hash_insert_check_StringSet,                        \
        StringStringHash*: hash_insert_check_StringStringHash,  \
        default: NULL)(h, key, b)

#define hash_remove(h, i)                            \
    _Generic(h,                  \
        Set*: hash_remove_Set,                  \
        Hash*: hash_remove_Hash,                  \
        StringHash*: hash_remove_StringHash,                  \
        BytesHash*: hash_remove_BytesHash,                  \
        StringPtrHash*: hash_remove_StringPtrHash,                  \
        StringSet*: hash_remove_StringSet,                  \
        StringStringHash*: hash_remove_StringStringHash,  \
        default: NULL)(h, i)

#define hash_free(h)                               \
    _Generic(h,                \
        Set*: hash_free_Set,                \
        Hash*: hash_free_Hash,                \
        StringHash*: hash_free_StringHash,                \
        BytesHash*: hash_free_BytesHash,                \
        StringPtrHash*: hash_free_StringPtrHash,                \
        StringSet*: hash_free_StringSet,                \
        StringStringHash*: hash_free_StringStringHash,  \
        default: NULL)(h)

#define hash_reset(h)                               \
    _Generic(h,                 \
        Set*: hash_reset_Set,                 \
        Hash*: hash_reset_Hash,                 \
        StringHash*: hash_reset_StringHash,                 \
        BytesHash*: hash_reset_BytesHash,                 \
        StringPtrHash*: hash_reset_StringPtrHash,                 \
        StringSet*: hash_reset_StringSet,                 \
        StringStringHash*: hash_reset_StringStringHash,  \
        default: NULL)(h)

#define hash_next(h, key)                          \
    _Generic(h,                \
        Set*: hash_next_Set,                \
        Hash*: hash_next_Hash,                \
        StringHash*: hash_next_StringHash,                \
        BytesHash*: hash_next_BytesHash,                \
        StringPtrHash*: hash_next_StringPtrHash,                \
        StringSet*: hash_next_StringSet,                \
        StringStringHash*: hash_next_StringStringHash,  \
        default: NULL)(h, key)

#define hash_get(h, key)                          \
    _Generic(h,               \
        Set*: hash_get_Set,               \
        Hash*: hash_get_Hash,               \
        StringHash*: hash_get_StringHash,               \
        BytesHash*: hash_get_BytesHash,               \
        StringPtrHash*: hash_get_StringPtrHash,               \
        StringSet*: hash_get_StringSet,               \
        StringStringHash*: hash_get_StringStringHash,  \
        default: NULL)(h, key)



#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define string_npos SIZE_MAX

#if defined(__GNUC__) || defined(__clang__)
#define STRING_FORMAT_CHECK(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define STRING_FORMAT_CHECK(fmt_idx, arg_idx)
#endif

[[nodiscard]]
static inline char* string_new(Allocator* allocator, size_t length, char const* data)
{
    char* str = array_new(allocator, char, length, length + 1);
    if (data)
    {
        memcpy(str, data, length);
    }
    str[length] = '\0';
    return str;
}

[[nodiscard]]
static inline char* string_clone(Allocator* allocator, char const* other)
{
    return string_new(allocator, array_size(other), other);
}

static inline void string_clear(char* str)
{
    if (array_size(str) == 0)
    {
        return;
    }
    array_resize(NULL, str, 0);
    str[0] = '\0';
}

static inline void string_free(Allocator* allocator, char* str)
{
    array_free(allocator, str);
}

[[nodiscard]]
static inline char* string_from_c_str(Allocator* allocator, char const* c_str)
{
    size_t len = c_str ? strlen(c_str) : 0;
    return string_new(allocator, len, c_str);
}

[[nodiscard]] STRING_FORMAT_CHECK(2, 0)
static inline char* string_from_vprint(Allocator* allocator, char const* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int num = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (num < 0) return NULL;
    char* result = string_new(allocator, (size_t)num, NULL);
    vsnprintf(result, num + 1, fmt, args);
    return result;
}

[[nodiscard]] STRING_FORMAT_CHECK(2, 3)
static inline char* string_from_print(Allocator* allocator, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* result = string_from_vprint(allocator, fmt, args);
    va_end(args);
    return result;
}

[[nodiscard("string_push may reallocate memory")]]
static inline char* string_push(Allocator* allocator, char* str, char ch)
{
    array_push(allocator, str, ch);
    array_push(allocator, str, '\0');
    array_size_lvalue(str) -= 1;
    return str;
}

[[nodiscard]]
static inline size_t string_length(char const* str)
{
    return array_size(str);
}

static inline char* string_ensure_space(Allocator* allocator, char* str, size_t old_len, size_t add_len)
{
    size_t old_capacity = array_capacity(str);
    assert((old_len == 0 && old_capacity == 0) || old_len < old_capacity);
    size_t new_size = old_len + add_len;
    if (old_capacity < new_size + 1)
    {
        char* new_str = string_new(allocator, new_size, NULL);
        if (old_len != 0)
        {
            memcpy(new_str, str, old_len);
        }
        array_free(allocator, str);
        return new_str;
    }
    else
    {
        array_size_lvalue(str) = new_size;
        return str;
    }
}

[[nodiscard("string_append_slice may reallocate memory")]]
static inline char* string_append_slice(Allocator* allocator, char* str1, size_t slice_len, char const* data)
{
    if (slice_len == 0)
    {
        return str1;
    }
    size_t str_len = string_length(str1);
    bool is_same_addr = (data >= str1 && data < str1 + str_len);
    char* new_str1 = string_ensure_space(allocator, str1, str_len, slice_len);
    if (is_same_addr)
    {
        data = new_str1 + (data - str1);
    }
    memcpy(new_str1 + str_len, data, slice_len);
    new_str1[str_len + slice_len] = '\0';
    return new_str1;
}

[[nodiscard("string_append_impl may reallocate memory")]]
static inline char* string_append_impl(Allocator* allocator, char* str1, char const* str2)
{
    return string_append_slice(allocator, str1, string_length(str2), str2);
}

[[nodiscard("string_append_c_str may reallocate memory")]]
static inline char* string_append_c_str(Allocator* allocator, char* str1, char const* str2)
{
    size_t str2_len = str2 ? strlen(str2) : 0;
    return string_append_slice(allocator, str1, str2_len, str2);
}

[[nodiscard("string_append_fmt may reallocate memory")]] STRING_FORMAT_CHECK(3, 4)
static inline char* string_append_fmt(Allocator* allocator, char* str, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* temp = string_from_vprint(allocator, fmt, args);
    va_end(args);
    if (!temp) return str;
    str = string_append_impl(allocator, str, temp);
    string_free(allocator, temp);
    return str;
}

[[nodiscard]]
static inline bool string_equal(char const* str1, char const* str2)
{
    return strcmp(str1, str2) == 0;
}

[[nodiscard("string_shrink_to_fit may reallocate memory")]]
static inline char* string_shrink_to_fit(Allocator* allocator, char* str)
{
    array_reserve(allocator, str, string_length(str) + 1);
    return str;
}

[[nodiscard]]
static inline size_t string_find_substr(char* str, size_t length, char const* data)
{
    if (length == 0) return string_npos;

    size_t str_len = string_length(str);
    if (str_len < length) return string_npos;

    char const* current = str;
    char const* end = str + str_len - length + 1;
    while ((current = (char const*)memchr(current, data[0], end - current)) != NULL)
    {
        if (memcmp(current, data, length) == 0)
        {
            return (size_t)(current - str);
        }
        current++;
    }

    return string_npos;
}

static inline bool string_starts_with(char const* str, char const* prefix)
{
    size_t len_prefix = strlen(prefix);
    return !strncmp(str, prefix, len_prefix);
}

static inline bool string_ends_with(char const* str, char const* suffix)
{
    if (!str || !suffix)
    {
        return false;
    }
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str)
    {
        return false;
    }
    return !strncmp(str + len_str - len_suffix, suffix, len_suffix);
}

static inline void string_toupper(char* str)
{
    while (*str)
    {
        *str = toupper(*str);
        ++str;
    }
}

static inline void string_tolower(char* str)
{
    while (*str)
    {
        *str = tolower(*str);
        ++str;
    }
}

static inline bool string_contains(char const* str, char const* sub_str)
{
    if (str == NULL && sub_str == NULL)
    {
        return true;
    }
    if (str == NULL || sub_str == NULL)
    {
        return false;
    }
    return strstr(str, sub_str);
}

#define string_printf(allocator, s, fmt, ...) ((s) = string_append_fmt(allocator, s, fmt, ##__VA_ARGS__))
#define string_putc(allocator, s, ch) ((s) = string_push(allocator, s, ch))
#define string_concat_c_str(allocator, s, c_str) ((s) = string_append_c_str(allocator, s, c_str))
#define string_append(allocator, s1, s2) ((s1) = string_append_impl(allocator, s1, s2))

#include <stdbool.h>

typedef struct Allocator Allocator;
typedef struct PathParser PathParser;

typedef enum PathParseStatus
{
    PARSE_STATUS_BEGIN = 0,
    PARSE_STATUS_AFTER_ROOT_NAME,
    PARSE_STATUS_AFTER_ROOT_DIRECTORY,
} PathParseStatus;

char* path_lexically_relative(char const* path, char const* base, Allocator* allocator);
char* path_lexically_normal(char const* path, Allocator* allocator);
char* path_root_path(char const* path, Allocator* allocator);
char* path_root_name(char const* path, Allocator* allocator);
char* path_root_directory(char const* path, Allocator* allocator);
char const* path_relative_path(char const* path);
char* path_parent_path(char const* path, Allocator* allocator);
char* path_filename(char const* path, Allocator* allocator);
char* path_stem(char const* path, Allocator* allocator);
char const* path_extension(char const* path);
char* path_replace_extension(char const* path, char const* ext, Allocator* allocator);
void* path_backslash_to_slash(char* path);
void path_slash_to_backslash(char* path);
char* path_combine(Allocator* allocator, char const* path, ... /* last must be NULL */);

bool path_is_absolute(char const* path);
bool path_is_empty(char const* path);
bool path_is_under_directory(char const* path, char const* directory);
bool path_has_relative_path(char const* path);
char* path_windows_style_to_linux_relative(char const* path, Allocator* allocator);

PathParser* path_create_parser(char const* path, Allocator* allocator);
PathParser* path_create_parser_with_status(char const* path, PathParseStatus status, Allocator* allocator);
char* path_next_element(PathParser* parser);
PathParseStatus path_get_parse_status(PathParser* parser);

#include <stdint.h>

typedef struct Allocator Allocator;

char const* utilities_split_cmd(Allocator* allocator, char const* cmd, char** out);
uint64_t utilities_compute_file_hash(char const* path);
char** utilities_copy_string_array(Allocator* allocator, char const** strings);



#include <stdarg.h>

typedef struct Allocator Allocator;

/// Format a string with curly-brace placeholders and variadic arguments.
///
/// Placeholders:
///   `{}`         — auto-indexed, consumes next variadic arg as string (const char*)
///   `{:s}`       — same as `{}`, explicitly typed string
///   `{:n}`       — arg is a Node*, outputs `node->name`
///   `{:d}`       — arg is an int, outputs decimal
///   `{0}` `{1}`  — positional index, type defaults to string
///   `{name}`     — named variable, resolved via get_var(); no variadic arg consumed
char const* fmt(char const* fmt_str, ...);
char* fmt_alloc_v(Allocator* allocator, char const* fmt_str, va_list* args);
char* fmt_alloc(Allocator* allocator, char const* fmt_str, ...);

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct Allocator Allocator;
typedef struct Executor Executor;
typedef struct Graph Graph;
typedef struct Cache Cache;
typedef struct Node Node;
typedef struct Node File;
typedef struct Node Exe;
typedef struct Node Cmd;
typedef struct Node ExeCmd;
typedef struct Node ThreadCmd;
typedef struct Task Task;
typedef int FnThread(Node* node);
typedef bool FnCheckDirty(Node* node);
typedef void FnProcessed(Node* node, Graph* graph);
typedef void FnBeforeExecute(Node* node);
typedef void FnAfterExecute(Node* node);

typedef enum NodeType
{
    NODE_TYPE_VIRTUAL,
    NODE_TYPE_FILE,
    NODE_TYPE_CMD,
} NodeType;

typedef enum FileType
{
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_SRC = 1,
    FILE_TYPE_OBJ,
    FILE_TYPE_EXE,
    FILE_TYPE_DLL,
    FILE_TYPE_LIB,
    FILE_TYPE_ENV,
    FILE_TYPE_MODULE_MAPPER,
} FileType;

typedef enum CmdType
{
    CMD_TYPE_EXECUTABLE,
    CMD_TYPE_THREAD,
} CmdType;

typedef enum VirtualExtType
{
    VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE = 1,
} VirtualExtType;

typedef enum OptionType
{
    OPTION_NONE,
    OPTION_EXE = 1,
    OPTION_INPUT,
    OPTION_OUTPUT,
    OPTION_FLAG,
    OPTION_BRIGHT_FLAG,
    OPTION_HIDDEN,
} OptionType;

struct Node
{
    union
    {
        struct
        {
            NodeType node_type : 3;
            uint32_t internal_flag : 1;
            uint32_t virtual_ext_type : 28;
        };
        union
        {
            struct
            {
                NodeType : 3;
                uint32_t : 1;
                FileType file_type : 5;
                uint32_t file_ext_type : 23;
            };
            struct
            {
                NodeType : 3;
                uint32_t : 1;
                CmdType cmd_type : 3;
                uint32_t cmd_ext_type : 25;
            };
        };
        uint32_t type;
    };
    uint32_t indegree;
    Node** dependencies;
    void (*prepare)(Node* node);
    void (*visit)(Node* node, Graph* graph, Executor* executor); // Run when visiting the DAG node
    bool (*check_dirty)(Node* node); // Run when start visiting
    void (*processed)(Node* node, Graph* graph);
    union
    {
        char* name;
        char* path;
    };
    void* extra_data;
    void* ctx;
    union
    {
        struct // file
        {
            uint64_t mtime;
            uint64_t content_hash;
            Node* build_cmd;
            char const** debugger_run_arguments;
            char const** test_entries;
            wchar_t* env_block;
            bool b_path_checked : 1;
            bool b_has_backslash : 1;
            bool b_has_space : 1;
        };
        struct // command
        {
            union
            {
                struct // process
                {
                    char* cmdline;
                    char* extra_options;
                    void (*write_stdout_line_fn)(Node* cmd, char const* line);
                    void (*write_stderr_line_fn)(Node* cmd, char const* line);
                    char* std_output;
                    char* std_error;
                    char* stdout_line;
                    char* stderr_line;
                    Node* env_node;
                    Node* make_cmdline;
                };
                struct // thread
                {
                    FnThread* fn;
                };
            };
            void (*before_execute)(Node* node);
            void (*after_execute)(Node* node);
            Node** inputs;
            Node** outputs;
            Node** implicit_inputs;
            char* description;
            int exit_code;
            char const* file;
            int line;
        };
    };
    bool b_default_excluded : 1;
    bool b_dynamic_indegree : 1;
    bool b_dirty : 1;
    bool b_prepared : 1;
};

typedef struct Obj
{
    struct Node;
    char const** link_libs;
    Node** link_nodes;
} Obj;

void init_node(void);

uint32_t node_make_file_type(FileType file_type, uint32_t ext_type);
uint32_t node_make_cmd_type(CmdType cmd_type, uint32_t ext_type);
uint32_t node_make_virtual_type(uint32_t ext_type);

bool node_virtual_check_dirty(Node* node);
void node_virtual_visit(Node* node, Graph* graph, Executor* executor);
void node_prepare(Node* node);
void node_visit(Node* node, Graph* graph, Executor* executor);
Node* node_create(uint32_t type, char const* name, size_t num_bytes);
void node_set_name(Node* node, char const* name);
void node_set_alias(Node* node, char const* alias);
void node_set_check_dirty_fn(Node* node, FnCheckDirty* fn);
void node_set_processed_fn(Node* node, FnProcessed* fn);
void node_set_extra_data(Node* node, void* extra_data);
void node_add_debugger_argument(Node* node, char const* arg);
void node_add_dependency(Node* node, Node* dependency);
void node_ensure_prepared(Node* node);

void cmd_prepare(Node* node);
void cmd_processed(Node* node, Graph* graph);
void cmd_visit(Node* node, Graph* graph, Executor* executor);
bool cmd_check_dirty(Node* node);
void cmd_before_execute(Node* node);
void cmd_update_output_mtime(Node* node);
void cmd_after_execute(Node* node);
void cmd_add_input(Node* node, Node* file);
void cmd_remove_input(Node* node, Node* file);
void cmd_add_output(Node* node, Node* file);
void cmd_remove_output(Node* node, Node* file);
void cmd_add_input_file_option(Node* node, char const* option, Node* file);
void cmd_add_output_file_option(Node* node, char const* option, Node* file);
void cmd_set_source_location(Node* node, char const* file, int line);
void cmd_write_stdout_line(Node* cmd, char const* line);
void cmd_write_stderr_line(Node* cmd, char const* line);
void cmd_set_write_output_line_fn(Node* node, void (*fn)(Node* cmd, char const* line));
void cmd_set_write_stderr_line_fn(Node* node, void (*fn)(Node* cmd, char const* line));
void cmd_set_before_execute_fn(Node* node, FnBeforeExecute* fn);
void cmd_set_after_execute_fn(Node* node, FnAfterExecute* fn);
void cmd_set_description(Node* node, char const* string);
void cmd_add_implicit_input(Node* node, char const* dep);
void cmd_add_option(Node* node, char const* option, char const* param, OptionType type);
void cmd_set_env(Node* node, Node* env);
char const* cmd_get_description(Node* node);
char const* cmd_get_cmdline(Node* node);
Task* cmd_create_task(Node* cmd, Executor* executor);
Node* file_create(char const* path, size_t num_bytes);
void file_processed(Node* node, Graph* graph);
void file_visit(Node* node, Graph* graph, Executor* executor);
bool file_check_dirty(Node* node);
bool file_path_has_space(Node* node);
bool file_path_has_backslash(Node* node);
uint64_t file_get_content_hash(Node* node);
char const* file_get_option_path(Node* node);
Node* find_node(char const* name);
bool has_dependency(Node* node, Node* dependency);
Node** get_all_nodes(void);
Node* get_or_add_file(char const* path);
Node* get_or_add_src(char const* path);
Node* get_or_add_file_with_type(char const* path, FileType type);
Node* get_or_add_node(uint32_t type, char const* name, size_t num_bytes);
Node* add_thread_cmd(FnThread* fn, void* ctx, char const* file, int line);
Node* add_process_cmd(char const* string, char const* file, int line);
Node* add_process_cmd_from_exe_node(Node* exe, char const* name, char const* file, int line);

#define FILE(fmt_str, ...) get_or_add_file(fmt(fmt_str, ##__VA_ARGS__))
#define LIB(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{lib_ext}", ##__VA_ARGS__), FILE_TYPE_LIB)
#define EXE(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{exe_ext}", ##__VA_ARGS__), FILE_TYPE_EXE)
#define DLL(fmt_str, ...) get_or_add_file_with_type(fmt(fmt_str "{dll_ext}", ##__VA_ARGS__), FILE_TYPE_DLL)
#define CALLBACK_CMD(fn, ctx) add_thread_cmd(fn, ctx, __FILE__, __LINE__)
#define CMD(cmdline) add_process_cmd(cmdline, __FILE__, __LINE__)
#define CMD_FROM_EXE(node, name) add_process_cmd_from_exe_node(node, name, __FILE__, __LINE__)

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct Allocator Allocator;
typedef struct Node Node;
typedef struct CCompileCmd CCompileCmd;
typedef struct StringPtrHash StringPtrHash;
typedef struct CompileCmdline CompileCmdline;

typedef enum ToolchainType
{
    TOOLCHAIN_TYPE_UNSPECIFIED = 0,
    TOOLCHAIN_TYPE_LLVM,
    TOOLCHAIN_TYPE_MSVC,
    TOOLCHAIN_TYPE_GCC,
    TOOLCHAIN_TYPE_ZIG,
    TOOLCHAIN_TYPE_TCC,
} ToolchainType;

static inline char const* get_toolchain_string(ToolchainType toolchain_type)
{
    switch (toolchain_type)
    {
    case TOOLCHAIN_TYPE_MSVC: return "msvc";
    case TOOLCHAIN_TYPE_LLVM: return "llvm";
    case TOOLCHAIN_TYPE_ZIG: return "zig";
    case TOOLCHAIN_TYPE_GCC: return "gcc";
    default: assert(false); return NULL;
    }
}

typedef enum ArchitectureType
{
    ARCH_UNSPECIFIED = 0,
    ARCH_X86 = 1,
    ARCH_X64 = 2,
    ARCH_ARM = 3,
    ARCH_ARM64 = 4,
    ARCH_COUNT,
} ArchitectureType;

static inline char const* get_arch_string(ArchitectureType arch)
{
    switch (arch)
    {
    case ARCH_X64: return "x64";
    case ARCH_X86: return "x86";
    case ARCH_ARM: return "arm";
    case ARCH_ARM64: return "arm64";
    default: return NULL;
    }
}

typedef enum OptimizationType
{
    OPTIMIZATION_TYPE_UNSPECIFIED = 0,
    OPTIMIZATION_TYPE_DEBUG,
    OPTIMIZATION_TYPE_RELEASE_FAST,
    OPTIMIZATION_TYPE_RELEASE_SMALL,
} OptimizationType;

static inline char const* get_optimization_string(OptimizationType optimization_type)
{
    switch (optimization_type)
    {
    case OPTIMIZATION_TYPE_DEBUG: return "debug";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "release_fast";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "release_small";
    case OPTIMIZATION_TYPE_UNSPECIFIED: return "release";
    default: return NULL;
    }
}

typedef enum CLanguageStandard
{
    C_LANGUAGE_STANDARD_UNSPECIFIED = 0,
    C_LANGUAGE_STANDARD_99,
    C_LANGUAGE_STANDARD_11,
    C_LANGUAGE_STANDARD_17,
    C_LANGUAGE_STANDARD_23,
} CLanguageStandard;

typedef enum CppLanguageStandard
{
    CPP_LANGUAGE_STANDARD_UNSPECIFIED = 0,
    CPP_LANGUAGE_STANDARD_98,
    CPP_LANGUAGE_STANDARD_11,
    CPP_LANGUAGE_STANDARD_14,
    CPP_LANGUAGE_STANDARD_17,
    CPP_LANGUAGE_STANDARD_20,
    CPP_LANGUAGE_STANDARD_23,
    CPP_LANGUAGE_STANDARD_26,
    CPP_LANGUAGE_STANDARD_COUNT,
} CppLanguageStandard;

static inline char const* get_cpp_std_string(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_98: return "cpp98";
    case CPP_LANGUAGE_STANDARD_11: return "cpp11";
    case CPP_LANGUAGE_STANDARD_14: return "cpp14";
    case CPP_LANGUAGE_STANDARD_17: return "cpp17";
    case CPP_LANGUAGE_STANDARD_20: return "cpp20";
    case CPP_LANGUAGE_STANDARD_23: return "cpp23";
    case CPP_LANGUAGE_STANDARD_26: return "cpp26";
    default: return NULL;
    }
}

typedef enum LinkerType
{
    LINKER_UNSPECIFIED,
    LINKER_LINK,
    LINKER_LD,
    LINKER_LLVM_LINK,
    LINKER_LLVM_LD,
    LINKER_LLVM_LLD,
    LINKER_ZIG_CC,
} LinkerType;

typedef enum SourceType
{
    SOURCE_TYPE_UNKNOWN,
    SOURCE_TYPE_C,
    SOURCE_TYPE_CPP,
    SOURCE_TYPE_CPPM,
    SOURCE_TYPE_ASM,
} SourceType;

typedef struct Src
{
    struct Node;
    SourceType source_type;
    Obj* default_obj;
} Src;

typedef struct TestExe
{
    struct Node;
    char const** entries;
} TestExe;

typedef struct BmiToObjCmd
{
    struct Node;
    CCompileCmd* c_compile_cmd;
} BmiToObjCmd;

void set_default_toolchain(ToolchainType type);
ToolchainType get_toolchain_by_current_compiler();
ToolchainType get_default_toolchain(void);
void set_llvm_linker_type(LinkerType type);
LinkerType get_llvm_linker_type(void);
void set_default_architecture(ArchitectureType type);
ArchitectureType get_default_architecture(void);
void set_default_optimization(OptimizationType type);
OptimizationType get_default_optimization(void);
void set_default_c_std(CLanguageStandard std);
CLanguageStandard get_default_c_std(void);
void set_default_cpp_std(CppLanguageStandard std);
CppLanguageStandard get_default_cpp_std(void);
void set_self_build_toolchain(ToolchainType toolchain);
void set_generate_vs_projects_enabled(bool enabled);
void set_generate_vscode_files_enabled(bool enabled);
void set_debug_info_enabled(bool b_enabled);
void set_test_enabled(bool b_enabled);
void set_msvc_show_include_prefix(char const* prefix);
void set_zig_target(char const* target);
void add_build_script(char const* path);
void add_build_script_search_directory(char const* directory);
Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch);
bool is_test_exe(Node* exe);
char const** get_test_entries(Node* exe);
Node* module_from_src(Node* src);

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line);
void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std);
void c_compile_cmd_add_include_directory(Node* node, char const* dir);
void c_compile_cmd_add_define(Node* node, char const* define);
void c_compile_cmd_add_flag(Node* node, char const* flag);
void c_compile_cmd_set_arch(Node* cmd, ArchitectureType arch);
void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export_name(Node* node, char const* name);
void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
void c_compile_cmd_add_self_build_options(Node* node);

// Obj
Obj* obj_create(char const* path);
Node* obj_from_src(Node* src);
Node* obj_from_src_with_variant(Node* src, char const* variant);
void obj_add_link_obj_from_src(Node* node, Node* src);
void obj_add_link_node(Node* node, Node* other);
void obj_add_link_lib(Node* node, char const* lib);
CCompileCmd* obj_get_compile_cmd(Node* obj);
char** obj_get_compile_includes(Node* obj, Allocator* allocator);
Node* get_default_obj(Node* src);

Node* link_cmd_create(Node* output, char const* file, int line);
void link_cmd_add_input(Node* cmd, Node* file);
void link_cmd_set_pdb(Node* cmd, Node* pdb);
Node* link_cmd_set_pdb_base_on_output(Node* cmd);
void link_cmd_set_out_import_lib(Node* cmd, Node* out_import_lib);
Node* link_cmd_get_out_import_lib(Node* cmd);
void link_cmd_set_def_file(Node* cmd, Node* def);
void link_cmd_add_lib(Node* cmd, char const* lib);
void link_cmd_add_lib_dir(Node* cmd, char const* directory);
void link_cmd_add_flag(Node* cmd, char const* flag);
void link_cmd_set_entry(Node* cmd, char const* name);
void link_cmd_set_arch(Node* cmd, ArchitectureType arch);
void link_cmd_set_toolchain_type(Node* cmd, ToolchainType toolchain_type);
void link_cmd_set_linker_type(Node* cmd, LinkerType linker_type);
void link_cmd_setup_self_build(Node* cmd);

Node* ar_cmd_create(Node* output, char const* file, int line);
void ar_cmd_add_input(Node* node, Node* input);
void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain);
Node* make_implib_cmd_create(Node* output, Node* def, ToolchainType toolchain_type, ArchitectureType architecture_type, char const* src_file_path, int line);

Node* get_or_add_src(char const* path);

#define LINK(output) link_cmd_create(output, __FILE__, __LINE__)
#define OBJ(src) get_default_obj(src)
#define SRC(fmt_str, ...) get_or_add_src(fmt(fmt_str, ##__VA_ARGS__))
#define CC(input, output) c_compile_cmd_create(input, output, __FILE__, __LINE__)
#define AR(output) ar_cmd_create(output, __FILE__, __LINE__)


typedef struct ArCmd ArCmd;
typedef struct Set Set;

struct ArCmd
{
    struct Node;
    ToolchainType toolchain;
    Node* output;
    Set* set_inputs;
    Node** ar_inputs;
};

Node* ar_cmd_create(Node* output, char const* file, int line);
void ar_cmd_add_input(Node* node, Node* input);
void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain);


typedef struct Node Node;
typedef struct StringPtrHash StringPtrHash;
typedef struct StringSet StringSet;
typedef struct CCompileCmd CCompileCmd;
typedef struct CompileCmdline CompileCmdline;
typedef struct ScanDepsCmd ScanDepsCmd;

struct CompileCmdline
{
    struct Node;
    CCompileCmd* cmd;
};

typedef struct CCompileCmd
{
    struct Node;
    Node* src;
    Node* out_obj;
    Node* pdb;
    ToolchainType toolchain;
    SourceType source_type;
    CLanguageStandard c_std;
    CppLanguageStandard cpp_std;
    OptimizationType optimization_type;
    ArchitectureType arch;
    StringSet* includes;
    StringSet* defines;
    char** flags;
    CompileCmdline* make_cmdline_cmd;
    bool b_cpp : 1;
    bool b_generate_debug_info : 1;
    bool b_cache_header_dependencies : 1;
    bool b_color_diagnostics : 1;
    bool b_scan_deps_dirty : 1;
    bool b_import_std : 1;

    // Cpp module
    Node* export_bmi;
    char* export_name;
    ScanDepsCmd* scan_deps_cmd;
    char** import_names;
    Node** import_bmis;
    StringPtrHash* export_map;
    StringPtrHash* import_map;

    // GCC
    Node* module_mapper;

    // MSVC
    char const* input_filename;
    char const* cwd;
    size_t show_include_prefix_len;
    char const* msvc_show_include_prefix;

} CCompileCmd;

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line);
void c_compile_cmd_add_include_directory(Node* node, char const* dir);
void c_compile_cmd_add_define(Node* node, char const* define);
void c_compile_cmd_add_flag(Node* node, char const* flag);
void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std);
void c_compile_cmd_set_cpp_std(Node* cmd, CppLanguageStandard cpp_std);
void c_compile_cmd_set_arch(Node* cmd, ArchitectureType arch);
void c_compile_cmd_set_optimization_type(Node* cmd, OptimizationType type);
void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi);
void c_compile_cmd_set_export_name(Node* node, char const* name);
void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
void c_compile_cmd_add_self_build_options(Node* node);
void c_compile_cmd_set_export_map(CCompileCmd* cmd, StringPtrHash* map);
void c_compile_cmd_set_import_map(CCompileCmd* cmd, StringPtrHash* map);
void compile_cmdline_node_make_cmdline(CompileCmdline* node);

typedef enum CToolchainNodeType
{
    C_CMD_COMPILE = 1,
    C_CMD_LINK,
    C_CMD_AR,
    C_CMD_MAKE_IMPLIB,
    C_CMD_BMI_TO_OBJ,
    C_CMD_SCAN_DEPS,
    C_CMD_SCAN_TESTS,
    C_VIRTUAL_MAKE_COMPILE_CMDLINE,
    C_FILE_TEST,
} CToolchainNodeType;


typedef struct Node Node;
typedef struct LinkCmd LinkCmd;

struct LinkCmd
{
    struct Node;
    Node* output;
    Node* pdb;
    Node* def;
    Node* out_import_lib;
    Node** input_option_files;
    char const** libs;
    char const** lib_directories;
    char const** flags;
    char* entry;
    ToolchainType toolchain;
    LinkerType linker_type;
    OptimizationType optimization;
    ArchitectureType arch;
    bool b_generate_debug_info;
    bool b_link_cpp;
};



#include <stdbool.h>

#include <stdint.h>
#include <stdio.h>

typedef struct Node Node;
typedef struct Cache Cache;
typedef struct CacheFile CacheFile;
typedef struct CacheRecordCmd CacheRecordCmd;
typedef struct CacheRecordCppModule CacheRecordCppModule;
typedef struct CacheRecordFile CacheRecordFile;
typedef struct CacheRecordTestExe CacheRecordTestExe;

enum CacheRecordType
{
    CACHE_RECORD_TYPE_EMPTY = 0,
    CACHE_RECORD_TYPE_IN_FILE = ENUM_CODE('I', 'N', 'F', 'E'),
    CACHE_RECORD_TYPE_OUT_FILE = ENUM_CODE('O', 'U', 'T', 'F'),
    CACHE_RECORD_TYPE_CMD = ENUM_CODE('C', 'C', 'M', 'D'),
    CACHE_RECORD_TYPE_CPP_MODULE = ENUM_CODE('C', 'P', 'P', 'M'),
    CACHE_RECORD_TYPE_TEST_EXE = ENUM_CODE('T', 'E', 'X', 'E'),

};

struct CacheRecordFile
{
    uint32_t id;
};

struct CacheRecordCmd
{
    char* name;
    char* cmdline;
    CacheFile* inputs;
    CacheFile* outputs;
    CacheFile* implicit_inputs;
};

struct CacheFile
{
    uint32_t id;
    uint64_t build_time;
    uint64_t content_hash;
};

struct CacheRecordCppModule
{
    uint32_t source_id;
    char* export;
    char** imports;
};

struct CacheRecordTestExe
{
    uint32_t source_id;
    char** entries;
};

struct Cache
{
    Allocator* allocator;
    StringPtrHash* hash_path_to_input_file_record;
    StringPtrHash* hash_path_to_output_file_record;
    StringPtrHash* hash_name_to_cmd_record;
    StringPtrHash* hash_source_path_to_cpp_module_record;
    StringPtrHash* hash_source_path_to_test_exe_record;
    char** strings;
    StringHash* hash_string_to_id;

    FILE* log_file;
    uint32_t total_records_read;
    uint32_t num_valid_records;

    bool b_needs_compaction;
};

Cache* get_cache(void);
Cache* cache_load(Allocator* allocator, char const* path);
Cache* cache_load_readonly(Allocator* allocator, char const* path);
void cache_destroy(Cache* c);
void cache_compact_log(Cache* c, char const* path);
char* cache_get_string(Cache* c, uint32_t id);
uint32_t cache_get_or_insert_string(Cache* c, char const* path);
CacheRecordFile* cache_find_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_find_out_file_record(Cache* c, char const* path);
CacheRecordTestExe* cache_find_test_exe(Cache* c, char const* source_path);
CacheRecordCppModule* cache_find_cpp_module_record(Cache* c, char const* source_path);
CacheRecordCmd* cache_find_cmd_record(Cache* c, char const* name);
CacheRecordFile* cache_add_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_add_out_file_record(Cache* c, char const* path);
CacheRecordFile* cache_get_or_add_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_get_or_add_out_file_record(Cache* c, char const* path);

void cache_write_cmd_record(Cache* c, CacheRecordCmd const* cmd);
void cache_write_cpp_module_record(Cache* c, CacheRecordCppModule const* module);
void cache_write_test_exe_record(Cache* c, CacheRecordTestExe const* exe);
bool cache_read_in_file_record(Cache* c);
bool cache_read_out_file_record(Cache* c);
bool cache_read_cmd_record(Cache* c);
bool cache_read_cpp_module_record(Cache* c);
bool cache_read_test_exe_record(Cache* c);
bool cache_read_records(Cache* c);

extern OptimizationType default_optimization_type;
extern ToolchainType default_toolchain;
extern bool b_generate_vscode_files;
extern char const* compile_commands_json_path;

void restart(void);

void init_var(void);
void set_var(char const* name, char const* value);
char const* get_var(char const* name);
void var_on_cwd_changed(void);


#include <stdbool.h>

typedef struct Directory Directory;
typedef struct Allocator Allocator;

typedef struct DirectoryEntry
{
    bool is_directory;
    char* name;
} DirectoryEntry;

Directory* directory_open(const char* path, Allocator* allocator);
DirectoryEntry* directory_read(Directory* dir);
void directory_close(Directory* dir);


typedef void FnEntry(void);

typedef struct Entry
{
    char const* name;
    FnEntry* fn;
    char const* file;
    int line;
    int priority;
} Entry;

static inline int entry_compare(void const* a, void const* b)
{
    Entry* entry_a = (Entry*)a;
    Entry* entry_b = (Entry*)b;
    return entry_a->priority - entry_b->priority;
}

Entry* entry_get_all(void);
void entry_push(Entry entry);
void entry_clean();

#define PRIORITY_BEFORE_DEFAULT 50
#define PRIORITY_DEFAULT 100
#define PRIORITY_AFTER_DEFAULT 200

#define ENTRY(fn, ...)                                  \
    CONSTRUCTOR(Entry_register_##fn)                    \
    void Entry_register_##fn(void)                      \
    {                                                   \
        void fn(void);                                  \
        Entry e = {#fn, fn, __FILE__, __LINE__, 0};     \
        e.priority = (PRIORITY_DEFAULT, ##__VA_ARGS__); \
        entry_push(e);                                  \
    }                                                   \
    void fn(void)


#include <stdbool.h>
#include <stdint.h>

typedef struct Node Node;
typedef struct Graph Graph;
typedef struct Hash Hash;
typedef struct Set Set;
typedef struct StringHash StringHash;
typedef char const* FnOutputFilter(Node* cmd, Allocator* allocator, char const* line);
typedef bool FnCheckDirty(Node* node);
typedef void FnVisitNode(Node* node);
typedef void FnBeforeExecute(Node* node);
typedef void FnAfterExecute(Node* node);

struct Graph
{
    Allocator* allocator;
    Node** sources;
    Hash* hash_node_to_next_set;
    Hash* hash_node_to_prev_set;
    Hash* hash_node_to_b_finished;
    Node** stack;
};

Graph* graph_create(Allocator* allocator, Node** targets, size_t num_targets);
void graph_destroy(Graph* graph);
void graph_add_target(Graph* graph, Node* node);
void graph_add_dynamic_edge(Graph* graph, Node* tail, Node* head);
void graph_set_node_processed(Graph* graph, Node* node);
Node* graph_pop(Graph* graph);
Node** graph_get_unreachable_nodes(Graph* graph, Allocator* allocator);

typedef struct Node Node;
typedef void FnAfterPrepare(void);

void set_root_dir(char const* dir);
void set_vs_project_version(char const* version);
void set_vscode_debugger_type(char const* type);
void set_after_prepare_callback(FnAfterPrepare* fn);
void set_cup_h_dir(char const* dir);
void set_node_default_excluded(bool b_default_excluded);
void set_content_hash_enabled(bool b_enabled);
ArchitectureType get_self_build_arch(void);
Node* add_copy_cmd(Node* input, Node* output, char const* file, int line);

int execute(void);

#define CUP_VERSION "1.0.0"

#define COPY(src, dst) add_copy_cmd(src, dst, __FILE__, __LINE__)

#include <stdbool.h>

typedef struct Allocator Allocator;
typedef struct Dylib Dylib;

Dylib* dylib_load(char const* name);
void dylib_unload(Dylib* lib);
void* dylib_get_symbol(Dylib* lib, char const* name);
void* dylib_get_image_base(Dylib* lib);


typedef struct CCompileCmd CCompileCmd;
typedef struct ScanDepsCmd ScanDepsCmd;

struct ScanDepsCmd
{
    struct Node;
    CCompileCmd* compile_cmd;
    char* export_name;
    char** imports;
};

Node* scan_deps_cmd_create(CCompileCmd* compile_cmd);

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Allocator Allocator;
typedef struct Task Task;
typedef struct Executor Executor;
typedef struct ExecutorSlot ExecutorSlot;

struct Task
{
    union
    {
        struct
        {
            char* cmdline;
            void (*write_stdout)(void* ctx, char const* buffer, size_t num_bytes);
            void (*write_stderr)(void* ctx, char const* buffer, size_t num_bytes);
            wchar_t* env_block;
        };
        struct
        {
            int (*thread_fn)(Task* task, void* ctx);
        };
    };
    void* ctx;
    int exit_code;
    bool b_thread;
};

Executor* executor_create(Allocator* allocator, size_t num_slots);
void executor_destroy(Executor* executor);
Task* executor_create_process_task(Executor* executor, char const* cmdline);
Task* executor_create_thread_task(Executor* executor, int (*fn)(Task*, void* ctx), void* ctx);
void executor_destroy_task(Executor* executor, Task* task);
void executor_set_task_write_stdout_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes));
void executor_set_task_write_stderr_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes));
void executor_set_task_context(Task* task, void* ctx);
void* executor_get_task_context(Task* task);
int executor_get_task_exit_code(Task* task);
void executor_add_task(Executor* executor, Task* task);
Task* executor_update(Executor* executor);
Task* executor_wait(Executor* executor);
bool executor_is_full(Executor* executor);
bool executor_is_empty(Executor* executor);

// Windows only
void executor_set_task_env_block(Task* task, wchar_t* env_block);
void executor_set_default_env(Executor* executor, wchar_t* env_block);

// private
void executor_execute_slot_thread(ExecutorSlot* slot);
void executor_execute_slot_process(ExecutorSlot* slot);
void executor_set_slot_task(Executor* executor, uint32_t slot_id, Task* task);
void executor_force_kill_task(ExecutorSlot* slot);
void executor_flush(Executor* executor);
uint32_t executor_find_empty_slot(Executor* executor);
uint32_t executor_find_finished_slot(Executor* executor);
uint32_t executor_find_task_slot_id(Executor* executor, Task* task);
ExecutorSlot* executor_get_slot(Executor* executor, uint32_t slot_id);

#include <stdbool.h>
#include <stdio.h>

typedef struct Allocator Allocator;

typedef enum
{
    DEPFILE_ITEM_TARGET,
    DEPFILE_ITEM_NORMAL_DEP,
    DEPFILE_ITEM_ORDER_ONLY_DEP,
    DEPFILE_ITEM_PHONY
} DepfileItemType;

typedef struct
{
    FILE* file;
    int state;
    bool is_phony_rule;
} DepfileParser;

void depfile_parser_init(DepfileParser* parser, FILE* f);
bool depfile_parser_next(DepfileParser* parser, struct Allocator* allocator, char** out_path, DepfileItemType* out_type);


#if CURRENT_PLATFORM == PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdbool.h>

#define OUTPUT_BUFFER_SIZE 1024

typedef struct Task Task;
typedef struct ReadPipeContext ReadPipeContext;
typedef struct ExecutorSlot ExecutorSlot;
typedef struct Executor Executor;
typedef struct Allocator Allocator;

struct ReadPipeContext
{
    OVERLAPPED overlapped;
    HANDLE read_pipe_handle;
    HANDLE child_write_pipe_handle;
    char buffer[OUTPUT_BUFFER_SIZE];
    void (*write_buffer)(void* ctx, char const* buffer, size_t num_bytes);
    void* write_buffer_ctx;
};

struct ExecutorSlot
{
    union
    {
        struct
        {
            HANDLE process;
            ReadPipeContext read_stdout_ctx;
            ReadPipeContext read_stderr_ctx;
        };
        struct
        {
            HANDLE thread;
            OVERLAPPED overlapped;
        };
    };
    Task* task;
    Executor* executor;
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    HANDLE iocp;
    wchar_t* default_env_block;
    wchar_t* current_env_block;
};

#endif


#if CURRENT_PLATFORM == PLATFORM_LINUX

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define OUTPUT_BUFFER_SIZE 1024

typedef struct Task Task;
typedef struct ReadPipeContext ReadPipeContext;
typedef struct ExecutorSlot ExecutorSlot;
typedef struct Executor Executor;
typedef struct Allocator Allocator;

struct ReadPipeContext
{
    int read_pipe;
    void (*write_buffer)(void* ctx, char const* buffer, size_t num_bytes);
    void* write_buffer_ctx;
};

typedef enum
{
    EPOLL_CTX_THREAD,
    EPOLL_CTX_STDOUT = EPOLL_CTX_THREAD,
    EPOLL_CTX_STDERR,
} EpollContextType;

struct ExecutorSlot
{
    union
    {
        EpollContextType ctx_thread;
        EpollContextType ctx_stdout;
    };
    EpollContextType ctx_stderr;
    union
    {
        struct
        {
            pid_t pid;
            ReadPipeContext read_stdout_ctx;
            ReadPipeContext read_stderr_ctx;
        };
        struct
        {
            pthread_t thread_id;
            int event_fd;
        };
    };
    Task* task;
    int epoll_fd;
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    int epoll_fd;
};

#endif


#if CURRENT_PLATFORM == PLATFORM_MACOS

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/event.h>
#include <sys/types.h>

#define OUTPUT_BUFFER_SIZE 1024

typedef struct Task Task;
typedef struct ReadPipeContext ReadPipeContext;
typedef struct ExecutorSlot ExecutorSlot;
typedef struct Executor Executor;
typedef struct Allocator Allocator;

struct ReadPipeContext
{
    int read_pipe;
    void (*write_buffer)(void* ctx, char const* buffer, size_t num_bytes);
    void* write_buffer_ctx;
};

typedef enum
{
    KQ_CTX_THREAD,
    KQ_CTX_STDOUT = KQ_CTX_THREAD,
    KQ_CTX_STDERR,
} KqueueContextType;

struct ExecutorSlot
{
    union
    {
        KqueueContextType ctx_thread;
        KqueueContextType ctx_stdout;
    };
    KqueueContextType ctx_stderr;
    union
    {
        struct
        {
            pid_t pid;
            ReadPipeContext read_stdout_ctx;
            ReadPipeContext read_stderr_ctx;
        };
        struct
        {
            pthread_t thread_id;
            int thread_done_pipe[2];
        };
    };
    Task* task;
    int kq_fd; // kqueue file descriptor
    bool b_finished;
};

struct Executor
{
    Allocator* allocator;
    ExecutorSlot* slots;
    size_t num_slots;
    size_t num_running_tasks;
    Task** pending_tasks;
    int kq_fd;
};

#endif

#define ANSI_COLOR_BLACK "\x1b[30m"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_GRAY "\x1b[90m"

#define ANSI_BRIGHT_BLACK "\x1b[90m"
#define ANSI_BRIGHT_RED "\x1b[91m"
#define ANSI_BRIGHT_GREEN "\x1b[92m"
#define ANSI_BRIGHT_YELLOW "\x1b[93m"
#define ANSI_BRIGHT_BLUE "\x1b[94m"
#define ANSI_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_BRIGHT_CYAN "\x1b[96m"
#define ANSI_BRIGHT_WHITE "\033[1;97m"

#define ANSI_BG_BLACK "\x1b[40m"
#define ANSI_BG_RED "\x1b[41m"
#define ANSI_BG_GREEN "\x1b[42m"
#define ANSI_BG_YELLOW "\x1b[43m"
#define ANSI_BG_BLUE "\x1b[44m"
#define ANSI_BG_MAGENTA "\x1b[45m"
#define ANSI_BG_CYAN "\x1b[46m"
#define ANSI_BG_WHITE "\x1b[47m"

#define ANSI_STYLE_BOLD "\x1b[1m"
#define ANSI_STYLE_DIM "\x1b[2m"
#define ANSI_STYLE_ITALIC "\x1b[3m"
#define ANSI_STYLE_UNDERLINE "\x1b[4m"
#define ANSI_STYLE_BLINK "\x1b[5m"

#define ANSI_COLOR_RESET "\x1b[0m"


typedef struct Node Node;

Node* module_from_src(Node* src);
Node* module_from_src_with_variant(Node* src, char const* variant);
Node* get_or_create_std_module_for_compile_cmd(CCompileCmd* cmd);
Node* get_or_create_std_compat_module_for_compile_cmd(CCompileCmd* cmd);


typedef struct CCompileCmd CCompileCmd;

struct ScanTestCmd
{
    struct Node;
    CCompileCmd* compile_cmd;
    Node* src;
    char** entries;
};


#include <stdint.h>

typedef struct EmbeddedFile EmbeddedFile;

struct EmbeddedFile
{
    char const* base64;
    size_t* size;
    char const* path;
    FileType type;
    size_t struct_bytes;
};

Node* create_gen_embedded_file_cmd(EmbeddedFile* embedded_file);


#if defined(BUILD_IMPLEMENTATION) || defined(MAIN_ENTRY)




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
    assert(block);
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
    assert(block);
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
    assert(new_block);
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
    Allocator* backend;
    TinyNode* head;
    Allocator interface;
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
    TinyAllocator* a = field_parent(TinyAllocator, allocator, interface);
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
    TinyAllocator* a = field_parent(TinyAllocator, allocator, interface);
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
    TinyAllocator* a = field_parent(TinyAllocator, allocator, interface);
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
    TinyAllocator* a = field_parent(TinyAllocator, allocator, interface);
    allocator_destroy(a->backend);
}

void tiny_reset(Allocator* allocator)
{
    TinyAllocator* a = field_parent(TinyAllocator, allocator, interface);
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
    assert(a);
    a->backend = backend;
    a->head = tiny_create_node(backend, size);
    a->limit = limit;
    a->interface.malloc = tiny_malloc;
    a->interface.calloc = tiny_calloc;
    a->interface.realloc = tiny_realloc;
    a->interface.free = tiny_free;
    a->interface.destroy = tiny_destroy;
    return &a->interface;
}

#include <ctype.h>

static char const* json_skip_space(char const* p)
{
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    return p;
}

JsonValue json_invalid();
char const* json_string(char const* p, JsonValue* v, Allocator* allocator);
char const* json_value(char const* p, JsonValue* v, Allocator* allocator);

static void json_object_add_key_value(JsonObject* object, char const* key, JsonValue value, Allocator* allocator)
{
    uint64_t index = array_size(object->keys);
    array_push(allocator, object->keys, key);
    array_push(allocator, object->values, value);
    hash_put(object->hash_name_to_index, key, index);
}

char const* json_object(char const* p, JsonValue* v, Allocator* allocator)
{
    *v = (JsonValue){.type = JSON_TYPE_OBJECT};
    v->object.hash_name_to_index = allocator_calloc(allocator, 1, sizeof(StringHash));
    v->object.hash_name_to_index->allocator = allocator;

    p += 1;

    for (;;)
    {
        p = json_skip_space(p);
        if (*p == '}')
        {
            p += 1;
            return p;
        }
        if (*p != '"')
        {
            *v = json_invalid();
            return p;
        }
        JsonValue name;
        p = json_string(p, &name, allocator);
        if (name.type == JSON_TYPE_INVALID)
        {
            *v = json_invalid();
            return p;
        }
        p = json_skip_space(p);
        if (*p != ':')
        {
            *v = json_invalid();
            return p;
        }
        p += 1;
        p = json_skip_space(p);
        JsonValue value;
        p = json_value(p, &value, allocator);
        if (value.type == JSON_TYPE_INVALID)
        {
            *v = json_invalid();
            return p;
        }
        json_object_add_key_value(&v->object, name.string, value, allocator);
        p = json_skip_space(p);
        if (*p == ',')
        {
            p += 1;
            p = json_skip_space(p);
            if (*p == '}')
            {
                *v = json_invalid();
                return p;
            }
            continue;
        }
        if (*p == '}')
        {
            p += 1;
            return p;
        }
        *v = json_invalid();
        return p;
    }
}

char const* json_array(char const* p, JsonValue* v, Allocator* allocator)
{
    *v = (JsonValue){.type = JSON_TYPE_ARRAY};
    p += 1;
    for (;;)
    {
        p = json_skip_space(p);
        if (*p == ']')
        {
            p += 1;
            return p;
        }
        JsonValue value;
        p = json_value(p, &value, allocator);
        if (value.type == JSON_TYPE_INVALID)
        {
            *v = json_invalid();
            return p;
        }
        array_push(allocator, v->array, value);
        p = json_skip_space(p);
        if (*p == ',')
        {
            p += 1;
            p = json_skip_space(p);
            if (*p == ']')
            {
                *v = json_invalid();
                return p;
            }
            continue;
        }
        if (*p == ']')
        {
            p += 1;
            return p;
        }
        *v = json_invalid();
        return p;
    }
}

char const* json_number(char const* p, JsonValue* v)
{
    v->type = JSON_TYPE_NUMBER;
    char* end;
    double d = strtod(p, &end);
    if (end == p)
    {
        v->type = JSON_TYPE_INVALID;
        return p;
    }
    v->number = d;
    return end;
}

static inline char const* json_u16(char const* p, uint16_t* u16)
{
    uint16_t value = 0;
    for (size_t i = 0; i != 4; i++)
    {
        unsigned char c = (unsigned char)p[i];
        if (!isxdigit(c))
        {
            *u16 = 0;
            return p;
        }
        value <<= 4;
        if (c >= '0' && c <= '9')
        {
            value |= c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            value |= c - 'a' + 10;
        }
        else
        {
            value |= c - 'A' + 10;
        }
    }
    *u16 = value;
    return p + 4;
}

static char const* json_string_push_utf16(JsonValue* json_string, char const* p, Allocator* allocator)
{
    uint16_t hex4;
    char const* end = json_u16(p, &hex4);
    if (end == p)
    {
        json_string->type = JSON_TYPE_INVALID;
        return p;
    }
    uint16_t u = hex4;
    uint32_t utf32;
    if (utf16_is_surrogate(u))
    {
        if (*end != '\\')
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        if (*(end + 1) != 'u')
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        p = end + 2;
        end = json_u16(p, &hex4);
        if (end == p)
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        uint16_t utf16[2] = {u, (uint16_t)hex4};

        int result = utf16_to_utf32(utf16, &utf32);
        if (result != 2)
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        char utf8[5];
        int n = utf8_encode(utf32, utf8);
        if (n == 0)
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        array_push_v(allocator, json_string->string, utf8, n);
        array_push(allocator, json_string->string, '\0');
        array_pop(json_string->string);
        return end;
    }
    else
    {
        utf16_to_utf32(&u, &utf32);
        char utf8[4];
        int n = utf8_encode(utf32, utf8);
        if (n == 0)
        {
            json_string->type = JSON_TYPE_INVALID;
            return p;
        }
        array_push_v(allocator, json_string->string, utf8, n);
        array_push(allocator, json_string->string, '\0');
        array_pop(json_string->string);
        return end;
    }
}

char const* json_string(char const* p, JsonValue* v, Allocator* allocator)
{
    *v = (JsonValue){.type = JSON_TYPE_STRING};
    p += 1;
    for (;;)
    {
        if (*p == '"')
        {
            p += 1;
            return p;
        }
        if (*p < 0x20)
        {
            *v = json_invalid();
            return p;
        }
        if (*p == '\\')
        {
            p += 1;
            switch (*p)
            {
            case '"':
            case '\\':
            case '/':
                string_putc(allocator, v->string, *p);
                p += 1;
                break;
            case 'b':
                string_putc(allocator, v->string, '\b');
                p += 1;
                break;
            case 'f':
                string_putc(allocator, v->string, '\f');
                p += 1;
                break;
            case 'n':
                string_putc(allocator, v->string, '\n');
                p += 1;
                break;
            case 'r':
                string_putc(allocator, v->string, '\r');
                p += 1;
                break;
            case 't':
                string_putc(allocator, v->string, '\t');
                p += 1;
                break;
            case 'u':
                p += 1;
                p = json_string_push_utf16(v, p, allocator);
                if (v->type == JSON_TYPE_INVALID)
                {
                    *v = json_invalid();
                    return p;
                }
                break;
            default:
                *v = json_invalid();
                return p;
            }
            continue;
        }
        string_putc(allocator, v->string, *p);
        p += 1;
    }
}

JsonValue json_invalid()
{
    return (JsonValue){.type = JSON_TYPE_INVALID};
}

JsonValue json_true()
{
    return (JsonValue){.type = JSON_TYPE_TRUE};
}

JsonValue json_false()
{
    return (JsonValue){.type = JSON_TYPE_FALSE};
}

JsonValue json_null()
{
    return (JsonValue){.type = JSON_TYPE_NULL};
}

char const* json_value(char const* p, JsonValue* v, Allocator* allocator)
{
    if (*p == '{')
    {
        p = json_object(p, v, allocator);
        return p;
    }
    if (*p == '[')
    {
        return json_array(p, v, allocator);
    }
    if (isdigit(*p) || *p == '-')
    {
        return json_number(p, v);
    }
    if (*p == '"')
    {
        return json_string(p, v, allocator);
    }
    if (*p == 't')
    {
        p += 1;
        char expected[] = "true";
        for (size_t i = 1; i < sizeof(expected) - 1; i++)
        {
            if (*p != expected[i])
            {
                *v = json_invalid();
                return p;
            }
            p += 1;
        }
        *v = json_true();
        return p;
    }
    if (*p == 'f')
    {
        p += 1;
        char expected[] = "false";
        for (size_t i = 1; i < sizeof(expected) - 1; i++)
        {
            if (*p != expected[i])
            {
                *v = json_invalid();
                return p;
            }
            p += 1;
        }
        *v = json_false();
        return p;
    }
    if (*p == 'n')
    {
        p += 1;
        char expected[] = "null";
        for (size_t i = 1; i < sizeof(expected) - 1; i++)
        {
            if (*p != expected[i])
            {
                *v = json_invalid();
                return p;
            }
            p += 1;
        }
        *v = json_null();
        return p;
    }
    *v = json_invalid();
    return p;
}

JsonValue json_from_string(char const* p, Allocator* allocator)
{
    if (p == NULL)
    {
        return json_invalid();
    }
    p = json_skip_space(p);
    JsonValue v;
    p = json_value(p, &v, allocator);
    p = json_skip_space(p);
    if (*p != '\0')
    {
        return json_invalid();
    }
    return v;
}

JsonValue* json_object_get_value(JsonObject* object, char const* key)
{
    uint32_t i = hash_index(object->hash_name_to_index, key);
    if (i == HASH_INVALID_INDEX)
    {
        return NULL;
    }
    uint32_t index = hash_value(object->hash_name_to_index, i);
    return &object->values[index];
}

#include <assert.h>
#include <time.h>

void os_ensure_dir_existed(char const* path)
{
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char const* dir = path_parent_path(path, stack_allocator);
    if (dir[0] && !os_file_exists(dir))
    {
        bool check = os_create_directory_tree(dir);
        assert(check);
    }
}

char* os_create_guid(Allocator* allocator, bool lowercase)
{
    const char* fmt = lowercase ? "%08x-%04x-%04x-%04x-%04x%04x%04x" : "%08X-%04X-%04X-%04X-%04X%04X%04X";

    union
    {
        struct
        {
            uint32_t a;
            uint16_t b;
            uint16_t c;
            uint16_t d;
            uint16_t e;
            uint16_t f;
            uint16_t g;
        };
        struct
        {
            uint64_t u64_1;
            uint64_t u64_2;
        };
    } guid = {.u64_1 = os_get_rand_uint64(), os_get_rand_uint64()};

    return string_from_print(allocator, fmt, guid.a, guid.b, guid.c, guid.d, guid.e, guid.f, guid.g);
}

char* os_read_all(Allocator* allocator, char const* path)
{
    FILE* f = os_fopen(path, "rb");
    if (f == NULL)
    {
        return NULL;
    }
    uint64_t size = os_get_file_size(path);
    if (size == UINT64_MAX)
    {
        fclose(f);
        return NULL;
    }
    char* buffer = NULL;
    array_resize(allocator, buffer, size + 1);
    size_t num = fread(buffer, 1, size, f);
    if (num != (size_t)size)
    {
        fclose(f);
        array_free(allocator, buffer);
        return NULL;
    }
    fclose(f);
    array_last(buffer) = '\0';
    array_pop(buffer);
    return buffer;
}

bool os_write_all(char const* path, char const* content, size_t size)
{
    FILE* f = os_fopen(path, "wb");
    if (f == NULL)
    {
        return false;
    }
    fwrite(content, 1, size, f);
    fclose(f);
    return true;
}

bool os_create_directory_tree(char const* path)
{
    Allocator* arena_allocator = allocator_create_chained();
    PathParser* parser = path_create_parser(path, arena_allocator);
    char* sub_path = NULL;
    while (true)
    {
        char* n = path_next_element(parser);
        if (n == NULL)
        {
            break;
        }
        if (sub_path == NULL)
        {
            sub_path = string_from_c_str(arena_allocator, n);
        }
        else
        {
            string_printf(arena_allocator, sub_path, "/%s", n);
        }
        if (path_get_parse_status(parser) == PARSE_STATUS_AFTER_ROOT_DIRECTORY)
        {
            if (!os_file_exists(sub_path))
            {
                if (!os_mkdir(sub_path))
                {
                    return false;
                }
            }
        }
    }
    allocator_destroy(arena_allocator);
    return true;
}

#include <assert.h>

struct PathParser
{
    Allocator* allocator;
    PathParseStatus status;
    char const* cursor;
};

enum PathElementType
{
    PATH_ELEMENT_NONE,
    PATH_ELEMENT_ROOT_NAME,
    PATH_ELEMENT_ROOT_DIRECTORY,
    PATH_ELEMENT_SEP,
    PATH_ELEMENT_FILE_NAME,
    PATH_ELEMENT_EMPTY,
};

struct PathElement
{
    Allocator* allocator;
    enum PathElementType type;
    char const* begin;
    char const* end;
    char* str;
};

void path_slash_to_backslash(char* p)
{
    while (*p)
    {
        if (*p == '/')
        {
            *p = '\\';
        }
        ++p;
    }
}

void path_replace_backslash(char* p)
{
    while (*p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
        ++p;
    }
}

void* path_backslash_to_slash(char* path)
{
    char* p = path;
    while (*p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
        ++p;
    }
    return path;
}

char const* path_skip_root_name(char const* p)
{
    assert(p);

    if (*p && *(p + 1) == ':')
    {
        return p + 2;
    }
    return p;
}

char const* path_skip_root_directory(char const* p)
{
    assert(p);

    if (*p == '/' || *p == '\\')
    {
        return p + 1;
    }
    return p;
}

char const* path_skip_root_path(char const* p)
{
    assert(p);
    p = path_skip_root_name(p);
    p = path_skip_root_directory(p);
    return p;
}

int path_root_name_length(char const* p)
{
    assert(p);
    if (*p && *(p + 1) == ':')
    {
        return 2;
    }
    return 0;
}

char* path_root_name(char const* p, Allocator* allocator)
{
    assert(p);
    int root_name_len = path_root_name_length(p);
    return string_from_print(allocator, "%.*s", root_name_len, p);
}

char* path_root_directory(char const* p, Allocator* allocator)
{
    assert(p);
    p = path_skip_root_name(p);
    char* root_directory = string_new(allocator, 0, NULL);
    while (*p == '/' || *p == '\\')
    {
        string_putc(allocator, root_directory, *p);
        ++p;
    }
    return root_directory;
}

char* path_root_path(char const* p, Allocator* allocator)
{
    assert(p);
    Allocator* temp_allocator = allocator_temp();
    char* root_name = path_root_name(p, temp_allocator);
    char* root_dir = path_root_directory(p, temp_allocator);
    char* root_path = string_from_print(allocator, "%s%s", root_name, root_dir);
    return root_path;
}

char const* path_relative_path(char const* p)
{
    assert(p);
    return path_skip_root_path(p);
}

bool path_has_relative_path(char const* p)
{
    assert(p);
    char const* relative_path = path_skip_root_path(p);
    return *relative_path;
}

bool path_is_empty(char const* p)
{
    assert(p);
    return *p == 0;
}

bool path_has_root_name(char const* p)
{
    assert(p);
    if (*p && *(p + 1) == ':')
    {
        return true;
    }
    return false;
}

bool path_has_root_directory(char const* p)
{
    assert(p);
    p = path_skip_root_name(p);
    if (*p == '/' || *p == '\\')
    {
        return true;
    }
    return false;
}

void path_parse_root_name(char const* p, struct PathElement* e)
{
    assert(p);
    assert(e);
    assert(e->allocator);

    if (*p && *(p + 1) == ':')
    {
        e->type = PATH_ELEMENT_ROOT_NAME;
        e->begin = p;
        e->end = p + 2;
        e->str = string_from_print(e->allocator, "%.2s", p);
    }
    else
    {
        e->type = PATH_ELEMENT_NONE;
    }
}

void path_parse_root_directory(char const* p, struct PathElement* e)
{
    assert(p);
    assert(e);
    assert(e->allocator);

    char const* q = p;
    while (*p == '/' || *p == '\\')
    {
        ++p;
    }
    if (p != q)
    {
        e->type = PATH_ELEMENT_ROOT_DIRECTORY;
        e->begin = q;
        e->end = p;
        e->str = string_from_print(e->allocator, "%.*s", (int)(p - q), q);
    }
    else
    {
        e->type = PATH_ELEMENT_NONE;
    }
}

void path_parse_relative_path(char const* p, struct PathElement* e, bool with_sep)
{
    assert(p);
    assert(e);
    assert(e->allocator);

    e->begin = p;
    while (*p == '/' || *p == '\\')
    {
        ++p;
    }
    if (with_sep && p - e->begin != 0)
    {
        e->end = p;
        if (*p == 0)
        {
            e->type = PATH_ELEMENT_EMPTY;
            e->str = string_from_c_str(e->allocator, "");
        }
        else
        {
            e->type = PATH_ELEMENT_SEP;
            e->str = string_from_print(e->allocator, "%.*s", (int)(e->end - e->begin), e->begin);
        }
        return;
    }
    if (*p == 0)
    {
        if (p == e->begin)
        {
            e->type = PATH_ELEMENT_NONE;
        }
        else
        {
            e->end = p;
            e->type = PATH_ELEMENT_EMPTY;
            e->str = string_from_c_str(e->allocator, "");
        }
    }
    else
    {
        e->begin = p;
        e->str = string_from_c_str(e->allocator, "");
        while (*p != '/' && *p != '\\' && *p)
        {
            string_putc(e->allocator, e->str, *p);
            ++p;
        }
        e->type = PATH_ELEMENT_FILE_NAME;
        e->end = p;
    }
}

void path_parse(char const* p, struct PathElement** elements, Allocator* allocator)
{
    assert(p);
    struct PathElement e = {.allocator = allocator};
    path_parse_root_name(p, &e);
    if (e.type != PATH_ELEMENT_NONE)
    {
        array_push(e.allocator, *elements, e);
        p = e.end;
    }
    path_parse_root_directory(p, &e);
    if (e.type != PATH_ELEMENT_NONE)
    {
        array_push(e.allocator, *elements, e);
        p = e.end;
    }
    for (;;)
    {
        path_parse_relative_path(p, &e, true);
        if (e.type != PATH_ELEMENT_NONE)
        {
            array_push(e.allocator, *elements, e);
            p = e.end;
        }
        else
        {
            break;
        }
    }
}

char* path_filename(char const* p, Allocator* allocator)
{
    assert(p);

    char const* relative_path = path_relative_path(p);
    if (*relative_path == 0)
    {
        return string_from_c_str(allocator, "");
    }

    Allocator* temp_allocator = allocator_temp();
    struct PathElement* elements = NULL;
    path_parse(p, &elements, temp_allocator);
    struct PathElement* e = &array_last(elements);
    char* result = string_from_c_str(allocator, e->str);
    return result;
}

char const* path_extension(char const* path)
{
    assert(path);
    char const* relative_path = path_relative_path(path);
    if (*relative_path == 0)
    {
        return relative_path;
    }
    Allocator* arena_allocator = allocator_temp();
    struct PathElement* elements = NULL;
    path_parse(path, &elements, arena_allocator);
    struct PathElement* last = &array_last(elements);
    char const* result = last->end;
    if (strcmp(last->str, ".") != 0 && strcmp(last->str, "..") != 0)
    {
        char const* p = last->begin;
        if (*p == '.')
        {
            ++p;
        }
        char const* ext = "";
        while (*p != 0)
        {
            if (*p == '.')
            {
                ext = p;
            }
            ++p;
        }
        result = ext;
    }
    return result;
}

char* path_replace_extension(char const* path, char const* ext, Allocator* allocator)
{
    assert(path);
    assert(ext);

    char const* p = path_extension(path);
    int len = p - path;
    char const* d = "";
    if (*ext != '.' && *ext != 0)
    {
        d = ".";
    }
    char* new_path = string_from_print(allocator, "%.*s%s%s", len, path, d, ext);
    return new_path;
}

char* path_stem(char const* path, Allocator* allocator)
{
    assert(path);

    Allocator* temp_allocator = allocator_temp();
    char* filename = path_filename(path, temp_allocator);
    if (strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0)
    {
        return string_from_c_str(allocator, filename);
    }
    char* p = filename;
    if (*p == '.')
    {
        ++p;
    }
    char* last_dot = strrchr(p, '.');
    if (last_dot)
    {
        char* result = NULL;
        array_push_v(allocator, result, filename, last_dot - filename);
        array_push(allocator, result, 0);
        array_pop(result);
        return result;
    }
    else
    {
        return string_from_c_str(allocator, filename);
    }
}

char path_guess_sep(char const* path)
{
    Allocator* temp_allocator = allocator_temp();
    char const* root_dir = path_root_directory(path, temp_allocator);
    if (*root_dir)
    {
        return *root_dir;
    }
    char const* p = path;
    while (*p != '/' && *p != '\\' && *p)
    {
        ++p;
    }
    return *p;
}

char* path_parent_path(char const* path, Allocator* allocator)
{
    char const* relative_path = path_relative_path(path);
    if (*relative_path == 0)
    {
        return string_from_c_str(allocator, path);
    }

    Allocator* arena_allocator = allocator_temp();
    char* result = string_from_c_str(allocator, "");
    struct PathElement* elements = NULL;
    path_parse(path, &elements, arena_allocator);
    struct PathElement* p = &array_last(elements);
    if (p->type == PATH_ELEMENT_EMPTY)
    {
        array_pop(elements);
    }
    else
    {
        while (array_size(elements) && p->type != PATH_ELEMENT_SEP && p->type != PATH_ELEMENT_ROOT_DIRECTORY)
        {
            array_pop(elements);
            --p;
        }
        if (p >= elements && p->type == PATH_ELEMENT_SEP)
        {
            array_pop(elements);
        }
    }

    for (size_t i = 0; i != array_size(elements); i++)
    {
        result = string_append_slice(allocator, result, strlen(elements[i].str), elements[i].str);
    }
    return result;
}

bool path_is_absolute(char const* path)
{
    Allocator* allocator = allocator_temp();
    char const* root = path_root_path(path, allocator);
    return *root;
}

bool path_is_relative(char const* path)
{
    return !path_is_absolute(path);
}

bool path_is_element_equal(char const* e1, char const* e2)
{
    if (string_equal(e1, e2))
    {
        return true;
    }
    return (*e1 == '/' || *e1 == '\\') && (*e2 == '/' || *e2 == '\\');
}

char* path_lexically_normal(char const* path, Allocator* allocator)
{
    if (*path == 0)
    {
        return string_from_c_str(allocator, "");
    }
    Allocator* arena_allocator = allocator_temp();
    char sep = path_guess_sep(path);
    char* result = string_from_c_str(allocator, "");

    char const* p = path;
    struct PathElement e = {.allocator = arena_allocator};
    path_parse_root_name(p, &e);
    if (e.type == PATH_ELEMENT_ROOT_NAME)
    {
        string_append(allocator, result, e.str);
        p = e.end;
    }
    path_parse_root_directory(p, &e);
    bool has_root_dir = false;
    if (e.type == PATH_ELEMENT_ROOT_DIRECTORY)
    {
        string_putc(allocator, result, sep);
        p = e.end;
        has_root_dir = true;
    }

    char** elements = NULL;
    char* n = NULL;
    PathParser* parser = path_create_parser(p, arena_allocator);
    while (true)
    {
        n = path_next_element(parser);
        if (n == NULL)
        {
            break;
        }
        if (strcmp(n, "..") == 0)
        {
            char* last = array_last(elements);
            if (array_size(elements) && strcmp(last, "..") != 0)
            {
                array_pop(elements);
            }
            else
            {
                if (!has_root_dir)
                {
                    array_push(arena_allocator, elements, n);
                }
            }
        }
        else
        {
            array_push(arena_allocator, elements, n);
        }
    }
    for (size_t i = 0; i != array_size(elements); i++)
    {
        if (*result && array_last(result) != sep && array_last(result) != ':')
        {
            string_putc(allocator, result, sep);
        }
        if (strcmp(elements[i], ".") != 0)
        {
            string_append(allocator, result, elements[i]);
        }
    }
    if (*result == 0)
    {
        string_putc(allocator, result, '.');
    }
    return result;
}

char* path_lexically_relative(char const* path, char const* base, Allocator* allocator)
{
    assert(path);
    assert(base);
    // Guess the path separator based on the path and base
    char sep = path_guess_sep(path);
    if (!sep)
    {
        sep = path_guess_sep(base);
    }
    if (!sep)
    {
        sep = '/';
    }

    Allocator* stack_allocator = allocator_temp();
    char const* root_name = path_root_name(path, stack_allocator);
    char const* base_root_name = path_root_name(base, stack_allocator);

    char const* relative_path = path_relative_path(path);
    char const* base_relative_path = path_relative_path(base);

    if (strcmp(root_name, base_root_name) != 0 ||
        path_is_absolute(path) != path_is_absolute(base) ||
        (path_has_root_directory(base) && !path_has_root_directory(path)) ||
        path_has_root_name(relative_path) ||
        path_has_root_name(base_relative_path))
    {
        return string_from_c_str(allocator, "");
    }
    PathParser* parser = path_create_parser(path, stack_allocator);
    PathParser* base_parser = path_create_parser(base, stack_allocator);
    int N = 0;

    char const* a;
    char const* b;

    while (true)
    {
        a = path_next_element(parser);
        b = path_next_element(base_parser);
        if (!a || !b || !path_is_element_equal(a, b))
        {
            break;
        }
    }
    while (b)
    {
        if (strcmp(b, "..") == 0)
        {
            --N;
        }
        else if (strcmp(b, ".") != 0 && strcmp(b, "") != 0)
        {
            ++N;
        }
        b = path_next_element(base_parser);
    }
    char* result = string_from_c_str(allocator, "");
    if (N == 0 && a == NULL)
    {
        string_putc(allocator, result, '.');
    }
    if (N >= 0)
    {
        for (int i = 0; i != N; i++)
        {
            result = string_append_slice(allocator, result, 2, "..");
            if (i != N - 1)
            {
                string_putc(allocator, result, sep);
            }
        }
        while (a)
        {
            if (*result)
            {
                string_putc(allocator, result, sep);
            }
            string_append(allocator, result, a);
            a = path_next_element(parser);
        }
    }
    return result;
}

char* path_combine(Allocator* allocator, char const* path, ...)
{
    assert(path);
    char sep = path_guess_sep(path);
    if (!sep)
    {
        sep = '/';
    }
    char* result = string_from_c_str(allocator, "");
    size_t len = strlen(path);
    char const* p = path;
    char const* q = path + len;
    if (len > 0)
    {
        if (*(q - 1) == '/' || *(q - 1) == '\\')
        {
            sep = *(q - 1);
            --q;
        }
    }
    if (q - p > 0)
    {
        string_printf(allocator, result, "%.*s", (int)(q - p), p);
    }
    va_list ap;
    va_start(ap, path);
    while (true)
    {
        char const* pa = va_arg(ap, char const*);
        if (pa == NULL)
        {
            va_end(ap);
            break;
        }
        len = strlen(pa);
        p = pa;
        q = pa + len;
        if (*p == '/' || *p == '\\')
        {
            p++;
        }
        if (q - p > 0)
        {
            string_printf(allocator, result, "%c%.*s", sep, (int)(q - p), p);
        }
    }

    return result;
}

bool path_is_under_directory(char const* path, char const* directory)
{
    Allocator* stack_allocator = allocator_temp();
    char* rel = path_lexically_relative(path, directory, stack_allocator);
    if (path_is_empty(rel))
    {
        return false;
    }
    PathParser* parser = path_create_parser_with_status(rel, PARSE_STATUS_AFTER_ROOT_DIRECTORY, stack_allocator);
    char const* start = path_next_element(parser);
    if (string_equal(start, ".."))
    {
        return false;
    }
    return true;
}

char* path_windows_style_to_linux_relative(char const* path, Allocator* allocator)
{
    char* result = string_from_c_str(allocator, "");
    Allocator* temp_allocator = allocator_temp();
    struct PathElement* elements = NULL;
    path_parse(path, &elements, temp_allocator);
    for (size_t i = 0; i != array_size(elements); i++)
    {
        if (elements[i].type == PATH_ELEMENT_ROOT_NAME)
        {
            if (*(elements[i].end - 1) == ':')
            {
                intptr_t last = elements[i].end - elements[i].begin - 1;
                elements[i].str[last] = 0;
            }
            string_printf(allocator, result, "%s", elements[i].str);
        }
        else if (elements[i].type == PATH_ELEMENT_FILE_NAME)
        {
            string_printf(allocator, result, "/%s", elements[i].str);
        }
    }
    return result;
}

PathParser* path_create_parser(char const* path, Allocator* allocator)
{
    PathParser* parser = allocator_malloc(allocator, sizeof(PathParser));
    parser->allocator = allocator;
    parser->cursor = path;
    parser->status = PARSE_STATUS_BEGIN;
    return parser;
}

PathParser* path_create_parser_with_status(char const* path, PathParseStatus status, Allocator* allocator)
{
    PathParser* parser = allocator_malloc(allocator, sizeof(PathParser));
    parser->allocator = allocator;
    parser->cursor = path;
    parser->status = status;
    return parser;
}

char* path_next_element(PathParser* parser)
{
    assert(parser && parser->cursor);

    struct PathElement e;
    e.allocator = parser->allocator;
    char const* p = parser->cursor;
    if (parser->status == PARSE_STATUS_BEGIN)
    {
        path_parse_root_name(p, &e);
        parser->status = PARSE_STATUS_AFTER_ROOT_NAME;
        if (e.type != PATH_ELEMENT_NONE)
        {
            parser->cursor = e.end;
            return e.str;
        }
    }
    if (parser->status == PARSE_STATUS_AFTER_ROOT_NAME)
    {
        path_parse_root_directory(p, &e);
        parser->status = PARSE_STATUS_AFTER_ROOT_DIRECTORY;
        if (e.type != PATH_ELEMENT_NONE)
        {
            parser->cursor = e.end;
            return e.str;
        }
    }
    if (parser->status == PARSE_STATUS_AFTER_ROOT_DIRECTORY)
    {
        path_parse_relative_path(p, &e, false);
        if (e.type != PATH_ELEMENT_NONE)
        {
            parser->cursor = e.end;
            return e.str;
        }
    }
    return NULL;
}

PathParseStatus path_get_parse_status(PathParser* parser)
{
    return parser->status;
}

#include <ctype.h>

char const* utilities_split_cmd(Allocator* allocator, char const* cmd, char** out)
{
    array_resize(allocator, *out, 0);
    if (!cmd)
    {
        return NULL;
    }
    char const* p = cmd;
    while (isspace(*p))
    {
        ++p;
    }
    if (*p == 0)
    {
        return NULL;
    }
    else if (*p == '"')
    {
        ++p;
        while (*p && *p != '"')
        {
            array_push(allocator, *out, *p);
            ++p;
        }
        if (*p == '"')
        {
            ++p;
        }
        array_push(allocator, *out, '\0');
        array_pop(*out);
    }
    else
    {
        while (*p && !isspace(*p))
        {
            array_push(allocator, *out, *p);
            ++p;
        }
        array_push(allocator, *out, '\0');
        array_pop(*out);
    }
    return p;
}

uint64_t utilities_compute_file_hash(char const* path)
{
    // FNV-1a
    uint64_t hash = 0xcbf29ce484222325ULL;
    FILE* file = os_fopen(path, "rb");
    if (!file)
    {
        return 0;
    }
    uint8_t buffer[4096];
    while (true)
    {
        size_t num_read = fread(buffer, 1, sizeof(buffer), file);
        if (num_read == 0)
        {
            break;
        }
        for (size_t i = 0; i < num_read; i++)
        {
            hash ^= (uint8_t)buffer[i];
            hash *= 0x100000001b3ULL;
        }
    }
    fclose(file);
    return hash;
}

char** utilities_copy_string_array(Allocator* allocator, char const** strings)
{
    if (strings == NULL)
    {
        return NULL;
    }

    char** result = array_new(allocator, char*, array_size(strings), 0);
    for (size_t i = 0; i != array_size(strings); i++)
    {
        result[i] = string_clone(allocator, strings[i]);
    }
    return result;
}


#include <stdio.h>

#include <assert.h>
#include <stdlib.h>

extern Allocator* node_allocator;

ToolchainType default_toolchain = TOOLCHAIN_TYPE_UNSPECIFIED;
ToolchainType self_build_toolchain = TOOLCHAIN_TYPE_UNSPECIFIED;
bool b_generate_debug_info = true;
bool b_test_enabled = false;
bool b_cache_header_dependencies = true;
ArchitectureType default_architecture_type = CURRENT_ARCHITECTURE;
OptimizationType default_optimization_type;
CLanguageStandard default_c_std;
CppLanguageStandard default_cpp_std;
char** build_script_search_directories;
StringSet* build_scripts = NULL;
char* msvc_show_include_prefix;
char* g_zig_target = NULL;
LinkerType linker_type = LINKER_UNSPECIFIED;
static bool b_linker_type_explicit = false;

static LinkerType c_toolchain_select_linker_automatically(ToolchainType toolchain)
{
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return LINKER_LINK;
    }
    else if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            return LINKER_LLVM_LLD;
        }
        else
        {
            return LINKER_LLVM_LD;
        }
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return LINKER_LD;
    }
    return LINKER_UNSPECIFIED;
}

static bool is_llvm_linker_type(LinkerType type)
{
    return type == LINKER_LLVM_LD || type == LINKER_LLVM_LLD || type == LINKER_LLVM_LINK;
}

void set_default_toolchain(ToolchainType type)
{
    default_toolchain = type;
    if (b_linker_type_explicit)
    {
        if (type != TOOLCHAIN_TYPE_LLVM)
        {
            error("LLVM linker selection requires the LLVM toolchain");
            exit(EXIT_FAILURE);
        }
        return;
    }
    linker_type = c_toolchain_select_linker_automatically(type);
}

ToolchainType get_default_toolchain(void)
{
    return default_toolchain;
}

void set_llvm_linker_type(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        error("set_llvm_linker_type only accepts LLVM -fuse-ld linker types");
        exit(EXIT_FAILURE);
    }
    if (default_toolchain != TOOLCHAIN_TYPE_UNSPECIFIED && default_toolchain != TOOLCHAIN_TYPE_LLVM)
    {
        return;
    }
    linker_type = type;
}

void c_toolchain_set_llvm_linker_type_explicit(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        error("c_toolchain_set_llvm_linker_type_explicit only accepts LLVM -fuse-ld linker types");
        exit(EXIT_FAILURE);
    }
    if (default_toolchain != TOOLCHAIN_TYPE_UNSPECIFIED && default_toolchain != TOOLCHAIN_TYPE_LLVM)
    {
        error("LLVM linker selection requires the LLVM toolchain");
        exit(EXIT_FAILURE);
    }
    linker_type = type;
    b_linker_type_explicit = true;
}

void c_toolchain_restore_llvm_linker_type(LinkerType type)
{
    if (!is_llvm_linker_type(type))
    {
        return;
    }
    linker_type = type;
}

LinkerType get_llvm_linker_type(void)
{
    if (!is_llvm_linker_type(linker_type))
    {
        return c_toolchain_select_linker_automatically(TOOLCHAIN_TYPE_LLVM);
    }
    return linker_type;
}

bool c_toolchain_is_linker_type_explicit(void)
{
    return b_linker_type_explicit;
}

void set_default_architecture(ArchitectureType type)
{
    default_architecture_type = type;
}

ArchitectureType get_default_architecture(void)
{
    return default_architecture_type;
}

void set_default_optimization(OptimizationType type)
{
    default_optimization_type = type;
}

OptimizationType get_default_optimization(void)
{
    return default_optimization_type;
}

void set_self_build_toolchain(ToolchainType toolchain)
{
    self_build_toolchain = toolchain;
}

ToolchainType get_toolchain_by_current_compiler()
{
    if (CURRENT_COMPILER == COMPILER_CLANG)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (CURRENT_COMPILER == COMPILER_CL)
    {
        return TOOLCHAIN_TYPE_MSVC;
    }
    if (CURRENT_COMPILER == COMPILER_GCC)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    return TOOLCHAIN_TYPE_UNSPECIFIED;
}

void set_debug_info_enabled(bool b_enabled)
{
    b_generate_debug_info = b_enabled;
}

void set_test_enabled(bool b_enabled)
{
    b_test_enabled = b_enabled;
}

void set_default_arch(ArchitectureType arch)
{
    default_architecture_type = arch;
}

void set_default_c_std(CLanguageStandard std)
{
    default_c_std = std;
}

CLanguageStandard get_default_c_std(void)
{
    return default_c_std;
}

void set_default_cpp_std(CppLanguageStandard std)
{
    default_cpp_std = std;
}

CppLanguageStandard get_default_cpp_std(void)
{
    return default_cpp_std;
}

void set_zig_target(char const* target)
{
    array_resize(allocator_c(), g_zig_target, 0);
    string_concat_c_str(allocator_c(), g_zig_target, target);
}

void set_msvc_show_include_prefix(char const* prefix)
{
    array_resize(allocator_c(), msvc_show_include_prefix, 0);
    string_concat_c_str(allocator_c(), msvc_show_include_prefix, prefix);
}

void add_build_script(char const* path)
{
    if (build_scripts == NULL)
    {
        build_scripts = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        build_scripts->allocator = node_allocator;
    }
    path = string_from_c_str(node_allocator, path);
    hash_insert(build_scripts, path);
}

void add_build_script_search_directory(char const* directory)
{
    char* dir = string_from_c_str(allocator_c(), directory);
    array_push(allocator_c(), build_script_search_directories, dir);
}

ToolchainType c_toolchain_select_toolchain_automatically();

static char const* c_toolchain_next_line(char const* str)
{
    char const* p = str;
    while (*p)
    {
        if (*p == '\r' && *(p + 1) == '\n')
        {
            p += 2;
            break;
        }
        if (*p == '\n') return p + 1;
        ++p;
    }
    return p;
}

char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
    char const* src_rel_path = src_path;
    if (path_is_absolute(src_path))
    {
        char const* cwd = os_get_cwd(temp_allocator);
        if (path_is_under_directory(src_path, cwd))
        {
            src_rel_path = path_lexically_relative(src_path, cwd, temp_allocator);
        }
        else
        {
            src_rel_path = path_windows_style_to_linux_relative(src_path, temp_allocator);
        }
    }
    char* obj_path;
    if (src_rel_path != src_path)
    {
        obj_path = fmt_alloc(allocator, "{out_dir}/{}/External/{}{}", sub_dir, src_rel_path, ext);
    }
    else
    {
        obj_path = fmt_alloc(allocator, "{out_dir}/{}/{}{}", sub_dir, src_rel_path, ext);
    }
    return obj_path;
}

Obj* obj_create(char const* path)
{
    uint32_t node_type = node_make_file_type(FILE_TYPE_OBJ, 0);
    Obj* obj = (Obj*)get_or_add_node(node_type, path, sizeof(Obj));
    return obj;
}

Node* obj_from_src(Node* src)
{
    Allocator* allocator = allocator_temp();
    char const* obj_dir = get_var("obj_dir");
    char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
    return (Node*)obj_create(obj_path);
}

Node* obj_from_src_with_variant(Node* src, char const* variant)
{
    Allocator* allocator = allocator_temp();
    char const* obj_dir = get_var("obj_dir");
    char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
    char const* stem = path_stem(obj_path, allocator_temp());
    char const* parent_path = path_parent_path(obj_path, allocator_temp());
    if (parent_path)
    {
        obj_path = fmt("{}/{}{}{}", parent_path, stem, variant, OBJ_EXT);
    }
    else
    {
        obj_path = fmt("{}{}{}", stem, variant, OBJ_EXT);
    }
    return (Node*)obj_create(obj_path);
}

Node* get_default_obj(Node* node)
{
    Src* src = (Src*)node;
    if (src->default_obj == NULL)
    {
        Allocator* allocator = allocator_temp();
        char const* obj_dir = get_var("obj_dir");
        char const* obj_path = determine_imtermediate_path(src->path, obj_dir, OBJ_EXT, allocator);
        src->default_obj = obj_create(obj_path);
    }
    return (Node*)src->default_obj;
}

void obj_add_link_obj_from_src(Node* node, Node* src)
{
    Node* obj = get_default_obj(src);
    obj_add_link_node(node, obj);
}

void obj_add_link_node(Node* node, Node* other)
{
    if (node->node_type != NODE_TYPE_FILE ||
        (node->file_type != FILE_TYPE_OBJ && node->file_type != FILE_TYPE_LIB))
    {
        assert(false && "An object link node can only be a .lib (library) or another .obj (object file).");
    }
    Obj* obj = (Obj*)node;
    array_push(node_allocator, obj->link_nodes, other);
}

void obj_add_link_lib(Node* node, char const* lib)
{
    Obj* obj = (Obj*)node;
    lib = string_from_c_str(node_allocator, lib);
    array_push(node_allocator, obj->link_libs, lib);
}

CCompileCmd* obj_get_compile_cmd(Node* obj)
{
    Node* cmd = obj->build_cmd;
    if (cmd == NULL)
    {
        return NULL;
    }
    CCompileCmd* cc = NULL;
    if (cmd->cmd_ext_type == C_CMD_COMPILE)
    {
        cc = (CCompileCmd*)cmd;
    }
    else if (cmd->cmd_ext_type == C_CMD_BMI_TO_OBJ)
    {
        BmiToObjCmd* cmd_bmi_to_obj = (BmiToObjCmd*)cmd;
        cc = cmd_bmi_to_obj->c_compile_cmd;
    }
    return cc;
}

char** obj_get_compile_includes(Node* obj, Allocator* allocator)
{
    char** includes = NULL;
    CCompileCmd* cc = obj_get_compile_cmd(obj);
    if (cc == NULL)
    {
        return NULL;
    }
    StringSet* s = cc->includes;
    for (uint32_t i = s->begin; i != s->end; i = hash_next(s, i))
    {
        char* inc = (char*)hash_key(s, i);
        array_push(allocator, includes, inc);
    }
    return includes;
}

bool is_test_exe(Node* exe)
{
    return exe->node_type == NODE_TYPE_FILE &&
           exe->file_type == FILE_TYPE_EXE &&
           exe->file_ext_type == C_FILE_TEST;
}

char const** get_test_entries(Node* exe)
{
    TestExe* test = (TestExe*)exe;
    return test->entries;
}

char const* get_architecture_string(ArchitectureType arch)
{
    if (arch == ARCH_X64)
    {
        return "x64";
    }
    else if (arch == ARCH_X86)
    {
        return "x86";
    }
    else if (arch == ARCH_ARM)
    {
        return "arm";
    }
    else if (arch == ARCH_ARM64)
    {
        return "arm64";
    }
    return NULL;
}

static char const* get_test_exe_path_for_src(Allocator* allocator, char const* src_path)
{
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return fmt_alloc(allocator, "{out_dir}/tests/{}" EXE_EXT, src_path);
    }
    else
    {
        return fmt_alloc(allocator, "{out_dir}/tests/{}.test", src_path);
    }
}

bool run_test_cmd_check_dirty(Node* node)
{
    return true;
}

void gen_test_src(void);

Node* add_test_exe_for_obj(Node* obj, char const** entries)
{
    Allocator* temp_allocator = allocator_temp();
    CCompileCmd* cc = (CCompileCmd*)obj->build_cmd;
    char const* path = get_test_exe_path_for_src(temp_allocator, cc->src->path);
    Node* exe = find_node(path);
    if (exe)
    {
        return exe;
    }
    uint32_t node_type = node_make_file_type(FILE_TYPE_EXE, C_FILE_TEST);
    exe = node_create(node_type, path, sizeof(TestExe));
    c_compile_cmd_add_define(obj->build_cmd, "BUILD_TEST");
    c_compile_cmd_add_include_directory(obj->build_cmd, get_var("test_src_dir"));
    Node* obj_test = OBJ(SRC("{test_src_dir}/cup/test.c"));
    obj_add_link_node(obj, obj_test);
    Node* test_h = FILE("{test_src_dir}/cup/test.h");
    node_add_dependency(obj->build_cmd, test_h);
    TestExe* test_exe = (TestExe*)exe;
    test_exe->entries = entries;
    Node* link = LINK(exe);
    cmd_set_source_location(link, obj->build_cmd->file, obj->build_cmd->line);
    link_cmd_add_input(link, obj);
    {
        Node* src = SRC("{test_src_dir}/cup/test_main.c");
        if (src->build_cmd == NULL)
        {
            gen_test_src();
        }
        Node* obj = OBJ(src);
        if (obj->build_cmd == NULL)
        {
            Node* cc = CC(src, obj);
            node_add_dependency(cc, test_h);
        }
        link_cmd_add_input(link, obj);
    }
    {
        Node* src = SRC("{test_src_dir}/cup/test.c");
        if (src->build_cmd == NULL)
        {
            gen_test_src();
        }
        Node* obj = OBJ(src);
        if (obj->build_cmd == NULL)
        {
            CC(src, obj);
        }
        link_cmd_add_input(link, obj);
    }

    return exe;
}

static void c_toolchain_lib_output_filter(Node* cmd, char const* line)
{
    if (string_starts_with(line, "   Creating library"))
    {
        return;
    }
    cmd_write_stdout_line(cmd, line);
}

Node* make_implib_cmd_create(Node* output, Node* def, ToolchainType toolchain_type, ArchitectureType architecture_type, char const* src_file_path, int line)
{
    Node* cmd = add_process_cmd(NULL, src_file_path, line);
    cmd->cmd_ext_type = C_CMD_MAKE_IMPLIB;
    cmd_set_source_location(cmd, src_file_path, line);
    Node* env = get_toolchain_env_node(toolchain_type, architecture_type);
    cmd_set_env(cmd, env);
    char const* lib_name = "llvm-lib";
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        lib_name = "lib";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        lib_name = "zig lib";
    }
    cmd_add_option(cmd, NULL, lib_name, OPTION_EXE);
    cmd_add_option(cmd, "/out:", output->path, OPTION_OUTPUT);
    cmd_add_option(cmd, "/def:", def->path, OPTION_INPUT);
    char const* arch;
    if (architecture_type == ARCH_X64)
    {
        arch = "x64";
    }
    else if (architecture_type == ARCH_X86)
    {
        arch = "x86";
    }
    else
    {
        error("Unsupported arch");
        exit(EXIT_FAILURE);
    }
    cmd_add_option(cmd, "/machine:", arch, OPTION_FLAG);
    cmd_add_input(cmd, def);
    cmd_add_output(cmd, output);
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        cmd_add_option(cmd, "/nologo", NULL, OPTION_HIDDEN);
        char const* exp_path = path_replace_extension(output->path, ".exp", allocator_temp());
        Node* exp = get_or_add_file(exp_path);
        cmd_add_output(cmd, exp);
        cmd_set_write_output_line_fn(cmd, c_toolchain_lib_output_filter);
    }
    return cmd;
}

void c_toolchain_rename_to_old(char const* path)
{
    Allocator* allocator = allocator_create_chained();
    if (path_is_absolute(path))
    {
        char const* cwd = get_var("workspace");
        if (!path_is_under_directory(path, cwd))
        {
            allocator_destroy(allocator);
            return;
        }
        else path = path_lexically_relative(path, cwd, allocator);
    }
    char* path_without_ext = path_replace_extension(path, "", allocator);
    char* old_path_base = fmt_alloc(allocator, "{out_dir}/old/{}", path_without_ext);
    char const* ext = path_extension(path);
    char* old_path = string_from_print(allocator, "%s%s", old_path_base, ext);
    int num = 0;
    FILE* try_file = NULL;
    while (true)
    {
        if (!os_file_exists(old_path)) break;
        try_file = os_fopen(old_path, "r+b");
        if (try_file != NULL) break;
        num++;
        array_resize(allocator, old_path, 0);
        string_printf(allocator, old_path, "%s.%d%s", old_path_base, num, ext);
    }
    if (try_file)
    {
        fclose(try_file);
    }
    os_ensure_dir_existed(old_path);
    if (!os_rename(path, old_path))
    {
        error("os_rename failed!");
        exit(EXIT_FAILURE);
    }
    allocator_destroy(allocator);
}

void init_toolchain(void)
{
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_default_toolchain(get_toolchain_by_current_compiler());
    }
    set_msvc_show_include_prefix("Note: including file:");
    compile_commands_json_path = "compile_commands.json";
    set_var("arch", get_architecture_string(default_architecture_type));
    char const* cfg = "release";
    if (default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED)
    {
        default_optimization_type = OPTIMIZATION_TYPE_DEBUG;
    }
    if (default_optimization_type == OPTIMIZATION_TYPE_DEBUG)
    {
        cfg = "debug";
    }
    set_var("cfg", cfg);
    set_var("test_src_dir", "src");
}

Node* get_or_add_src(char const* path)
{
    Node* node = find_node(path);
    if (!node)
    {
        uint32_t node_type = node_make_file_type(FILE_TYPE_SRC, 0);
        node = node_create(node_type, path, sizeof(Src));
    }
    return node;
}

Cache* cache = NULL;

static const uint32_t CACHE_MAGIC = 0x43505543; // "CUPC" (Little Endian)
static const uint32_t CACHE_VERSION = 2;

#define cache_init_hash(var)                                  \
    (var) = allocator_calloc(allocator, 1, sizeof((var)[0])); \
    (var)->allocator = allocator

Cache* cache_create(Allocator* allocator)
{
    Cache* c = allocator_calloc(allocator, 1, sizeof(Cache));
    c->allocator = allocator;
    cache_init_hash(c->hash_string_to_id);
    cache_init_hash(c->hash_path_to_input_file_record);
    cache_init_hash(c->hash_path_to_output_file_record);
    cache_init_hash(c->hash_name_to_cmd_record);
    cache_init_hash(c->hash_source_path_to_cpp_module_record);
    cache_init_hash(c->hash_source_path_to_test_exe_record);
    return c;
}

#undef cache_init_hash

static bool cache_read_u32(FILE* f, uint32_t* v)
{
    return fread(v, sizeof(uint32_t), 1, f) == 1;
}

static bool cache_file_remaining(FILE* f, uint64_t* out_size)
{
    long pos = ftell(f);
    if (pos < 0) return false;
    if (fseek(f, 0, SEEK_END) != 0) return false;
    long end = ftell(f);
    if (fseek(f, pos, SEEK_SET) != 0) return false;
    if (end < pos) return false;
    *out_size = (uint64_t)(end - pos);
    return true;
}

static bool cache_file_has_remaining(FILE* f, uint64_t size)
{
    uint64_t remaining = 0;
    if (!cache_file_remaining(f, &remaining)) return false;
    return remaining >= size;
}

static bool cache_read_bytes(FILE* f, void* data, size_t size)
{
    if (size == 0) return true;
    if (!cache_file_has_remaining(f, size)) return false;
    return fread(data, 1, size, f) == size;
}

static bool cache_read_str(FILE* f, Allocator* allocator, char** out_str)
{
    uint32_t len = 0;
    if (!cache_read_u32(f, &len)) return false;
    if (!cache_file_has_remaining(f, len)) return false;
    char* str = string_new(allocator, len, NULL);
    if (len > 0)
    {
        if (!cache_read_bytes(f, str, len))
        {
            string_free(allocator, str);
            return false;
        }
    }
    *out_str = str;
    return true;
}

static bool cache_write_u32(FILE* f, uint32_t v)
{
    return fwrite(&v, sizeof(uint32_t), 1, f) == 1;
}

static bool cache_write_str(FILE* f, char const* str)
{
    uint32_t len = (uint32_t)(str ? strlen(str) : 0);
    if (!cache_write_u32(f, len)) return false;
    if (len > 0)
    {
        return fwrite(str, 1, len, f) == len;
    }
    return true;
}

void init_cache(void)
{
    if (cache == NULL)
    {
        char const* path = fmt("{out_dir}/.cup_cache");
        cache = cache_load(allocator_c(), path);
    }
}

Cache* get_cache(void)
{
    return cache;
}

size_t cache_calc_num_records(Cache* c)
{
    size_t num_records = 0;
    num_records += c->hash_path_to_input_file_record->size;
    num_records += c->hash_path_to_output_file_record->size;
    num_records += c->hash_name_to_cmd_record->size;
    num_records += c->hash_source_path_to_cpp_module_record->size;
    num_records += c->hash_source_path_to_test_exe_record->size;
    return num_records;
}

Cache* cache_load_readonly(Allocator* allocator, char const* path)
{
    Cache* c = cache_create(allocator);
    FILE* f = os_fopen(path, "r+b");
    bool b_invalid_cache = false;
    if (!f)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    uint32_t magic = 0, version = 0;
    if (!cache_read_u32(f, &magic) || magic != CACHE_MAGIC)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    if (!cache_read_u32(f, &version) || version != CACHE_VERSION)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    c->log_file = f;
    if (!cache_read_records(c))
    {
        b_invalid_cache = true;
    }
    else
    {
        c->num_valid_records = cache_calc_num_records(c);
    }
EndLoad:
    if (f)
    {
        fclose(f);
    }
    c->log_file = NULL;
    if (b_invalid_cache)
    {
        cache_destroy(c);
        c = cache_create(allocator);
    }
    return c;
}

void cache_clear(Cache* c);

Cache* cache_load(Allocator* allocator, char const* path)
{
    os_ensure_dir_existed(path);
    Cache* c = cache_create(allocator);
    bool b_invalid_cache = false;
    FILE* f = os_fopen(path, "r+b");
    if (!f)
    {
        goto OpenLog;
    }
    uint32_t magic = 0, version = 0;
    if (!cache_read_u32(f, &magic) || magic != CACHE_MAGIC)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    if (!cache_read_u32(f, &version) || version != CACHE_VERSION)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    c->log_file = f;
    if (!cache_read_records(c))
    {
        b_invalid_cache = true;
    }
    else
    {
        c->num_valid_records = cache_calc_num_records(c);
    }
EndLoad:
    if (f)
    {
        fclose(f);
    }
    c->log_file = NULL;
    if (b_invalid_cache)
    {
        error("Invalid cache, deleting: %s", path);
        cache_clear(c);
        allocator_free(allocator, c);
        os_remove_file(path);
        c = cache_create(allocator);
    }
OpenLog:
    if (c->total_records_read > c->num_valid_records * 2)
    {
        c->b_needs_compaction = true;
    }
    c->log_file = os_fopen(path, "ab");
    assert(c->log_file);
    fseek(c->log_file, 0, SEEK_END);
    if (c->log_file && ftell(c->log_file) == 0)
    {
        cache_write_u32(c->log_file, CACHE_MAGIC);
        cache_write_u32(c->log_file, CACHE_VERSION);
        fflush(c->log_file);
    }
    return c;
}

static void cache_destroy_record_test_exe(Allocator* allocator, CacheRecordTestExe* record)
{
    for (size_t i = 0; i != array_size(record->entries); i++)
    {
        char* entry = record->entries[i];
        array_free(allocator, entry);
    }
    allocator_free(allocator, record);
}

static void cache_destroy_record_cpp_module(Allocator* allocator, CacheRecordCppModule* record)
{
    for (size_t i = 0; i != array_size(record->imports); i++)
    {
        char* name = record->imports[i];
        array_free(allocator, name);
    }
    array_free(allocator, record->export);
    allocator_free(allocator, record);
}

static void cache_destroy_record_cmd(Allocator* allocator, CacheRecordCmd* record)
{
    array_free(allocator, record->implicit_inputs);
    array_free(allocator, record->outputs);
    array_free(allocator, record->inputs);
    string_free(allocator, record->cmdline);
    string_free(allocator, record->name);
}

void cache_destroy_files(Cache* c)
{
    StringPtrHash* h;
    h = c->hash_path_to_output_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        allocator_free(c->allocator, record);
    }
    hash_free(c->hash_path_to_output_file_record);
    h = c->hash_path_to_input_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        allocator_free(c->allocator, record);
    }
    hash_free(c->hash_path_to_input_file_record);
    for (size_t i = 0; i != array_size(c->strings); i++)
    {
        char* path = c->strings[i];
        string_free(c->allocator, path);
    }
    hash_free(c->hash_string_to_id);
    array_free(c->allocator, c->strings);
}

void cache_clear(Cache* c)
{
    if (c->log_file)
    {
        fclose(c->log_file);
    }

    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordTestExe* record = hash_value(h, i);
        cache_destroy_record_test_exe(c->allocator, record);
    }
    hash_free(c->hash_source_path_to_test_exe_record);
    h = c->hash_source_path_to_cpp_module_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCppModule* record = hash_value(h, i);
        cache_destroy_record_cpp_module(c->allocator, record);
    }
    hash_free(c->hash_source_path_to_cpp_module_record);

    h = c->hash_name_to_cmd_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCmd* record = hash_value(h, i);
        cache_destroy_record_cmd(c->allocator, record);
    }
    hash_free(c->hash_name_to_cmd_record);
    cache_destroy_files(c);
}

void cache_destroy(Cache* c)
{
    cache_clear(c);
    allocator_free(c->allocator, c);
}

static CacheRecordFile* cache_merge_file_record(Cache* c, char const* path, StringPtrHash* h)
{
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    if (b_existed)
    {
        return hash_value(h, i);
    }
    CacheRecordFile* record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordFile));
    record->id = cache_get_or_insert_string(c, path);
    hash_key(h, i) = c->strings[record->id];
    hash_value(h, i) = record;
    return record;
}

CacheRecordFile* cache_merge_in_file_record(Cache* c, char const* path)
{
    return cache_merge_file_record(c, path, c->hash_path_to_input_file_record);
}

CacheRecordFile* cache_merge_out_file_record(Cache* c, char const* path)
{
    return cache_merge_file_record(c, path, c->hash_path_to_output_file_record);
}

static void cache_copy_cmd_record(Allocator* allocator, CacheRecordCmd* dst, CacheRecordCmd const* src)
{
    if (dst == src)
    {
        return;
    }
    string_clear(dst->name);
    string_concat_c_str(allocator, dst->name, src->name);
    string_clear(dst->cmdline);
    string_concat_c_str(allocator, dst->cmdline, src->cmdline);
    size_t num_inputs = array_size(src->inputs);
    array_resize(allocator, dst->inputs, 0);
    array_push_v(allocator, dst->inputs, src->inputs, num_inputs);
    size_t num_outputs = array_size(src->outputs);
    array_resize(allocator, dst->outputs, 0);
    array_push_v(allocator, dst->outputs, src->outputs, num_outputs);
    size_t num_implicit_inputs = array_size(src->implicit_inputs);
    array_resize(allocator, dst->implicit_inputs, 0);
    array_push_v(allocator, dst->implicit_inputs, src->implicit_inputs, num_implicit_inputs);
}

void cache_merge_cmd_record(Cache* c, CacheRecordCmd const* cmd)
{
    StringPtrHash* h = c->hash_name_to_cmd_record;
    uint32_t i = hash_index(h, cmd->name);
    if (i != UINT32_MAX)
    {
        CacheRecordCmd* record = hash_value(h, i);
        cache_copy_cmd_record(c->allocator, record, cmd);
        hash_key(h, i) = record->name;
        return;
    }

    CacheRecordCmd* record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordCmd));
    cache_copy_cmd_record(c->allocator, record, cmd);
    bool b_existed = false;
    i = hash_insert_check(h, record->name, &b_existed);
    assert(!b_existed);
    hash_key(h, i) = record->name;
    hash_value(h, i) = record;
}

static void cache_copy_cpp_module_record(Allocator* allocator, CacheRecordCppModule* dst, CacheRecordCppModule const* src)
{
    if (dst == src)
    {
        return;
    }
    dst->source_id = src->source_id;
    string_clear(dst->export);
    string_concat_c_str(allocator, dst->export, src->export);
    size_t old_size = array_size(dst->imports);
    size_t new_size = array_size(src->imports);
    for (size_t i = 0; i != new_size; i++)
    {
        char* import_name = src->imports[i];
        if (i >= old_size)
        {
            char* copied = string_from_c_str(allocator, import_name);
            array_push(allocator, dst->imports, copied);
        }
        else
        {
            string_clear(dst->imports[i]);
            string_concat_c_str(allocator, dst->imports[i], import_name);
        }
    }
    if (old_size > new_size)
    {
        for (size_t i = new_size; i != old_size; i++)
        {
            array_free(allocator, dst->imports[i]);
        }
    }
    array_resize(allocator, dst->imports, new_size);
}

void cache_merge_cpp_module_record(Cache* c, CacheRecordCppModule const* module)
{
    StringPtrHash* h = c->hash_source_path_to_cpp_module_record;
    char const* path = cache_get_string(c, module->source_id);
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    CacheRecordCppModule* record = hash_value(h, i);
    if (!b_existed)
    {
        record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordCppModule));
        hash_value(h, i) = record;
    }
    cache_copy_cpp_module_record(c->allocator, record, module);
}

static void cache_copy_test_exe_record(Allocator* allocator, CacheRecordTestExe* dst, CacheRecordTestExe const* src)
{
    if (dst == src)
    {
        return;
    }
    dst->source_id = src->source_id;
    size_t old_size = array_size(dst->entries);
    size_t new_size = array_size(src->entries);
    for (size_t i = 0; i != array_size(src->entries); i++)
    {
        char* name = src->entries[i];
        if (i >= old_size)
        {
            char* copied = string_from_c_str(allocator, name);
            array_push(allocator, dst->entries, copied);
        }
        else
        {
            string_clear(dst->entries[i]);
            string_concat_c_str(allocator, dst->entries[i], name);
        }
    }
    if (old_size > new_size)
    {
        for (size_t i = new_size; i != old_size; i++)
        {
            array_free(allocator, dst->entries[i]);
        }
    }
    array_resize(allocator, dst->entries, new_size);
}

void cache_merge_test_exe_record(Cache* c, CacheRecordTestExe const* exe)
{
    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    char const* path = cache_get_string(c, exe->source_id);
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    CacheRecordTestExe* record = hash_value(h, i);
    if (!b_existed)
    {
        record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordTestExe));
        hash_value(h, i) = record;
    }
    cache_copy_test_exe_record(c->allocator, record, exe);
}

CacheRecordFile* cache_find_file_record(Cache* c, char const* path, StringPtrHash* h)
{
    uint32_t i = hash_index(h, path);
    if (i == HASH_INVALID_INDEX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

char* cache_get_string(Cache* c, uint32_t id)
{
    return c->strings[id];
}

uint32_t cache_get_or_insert_string(Cache* c, char const* path)
{
    StringHash* h = c->hash_string_to_id;
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    if (!b_existed)
    {
        hash_value(h, i) = array_size(c->strings);
        char* new_path = string_from_c_str(c->allocator, path);
        hash_key(h, i) = new_path;
        array_push(c->allocator, c->strings, new_path);
    }
    return hash_value(h, i);
}

CacheRecordFile* cache_find_in_file_record(Cache* c, char const* path)
{
    return cache_find_file_record(c, path, c->hash_path_to_input_file_record);
}

CacheRecordFile* cache_find_out_file_record(Cache* c, char const* path)
{
    return cache_find_file_record(c, path, c->hash_path_to_output_file_record);
}

CacheRecordCppModule* cache_find_cpp_module_record(Cache* c, char const* source_path)
{
    StringPtrHash* h = c->hash_source_path_to_cpp_module_record;
    uint32_t i = hash_index(h, source_path);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordTestExe* cache_find_test_exe(Cache* c, char const* source_path)
{
    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    uint32_t i = hash_index(h, source_path);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordCmd* cache_find_cmd_record(Cache* c, char const* name)
{
    StringPtrHash* h = c->hash_name_to_cmd_record;
    uint32_t i = hash_index(h, name);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordFile* cache_add_in_file_record(Cache* c, char const* path)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_IN_FILE;
    cache_write_u32(f, type);
    cache_write_str(f, path);
    return cache_merge_in_file_record(c, path);
}

CacheRecordFile* cache_get_or_add_in_file_record(Cache* c, char const* path)
{
    CacheRecordFile* record = cache_find_in_file_record(c, path);
    if (!record)
    {
        record = cache_add_in_file_record(c, path);
    }
    return record;
}

CacheRecordFile* cache_get_or_add_out_file_record(Cache* c, char const* path)
{
    CacheRecordFile* record = cache_find_out_file_record(c, path);
    if (!record)
    {
        record = cache_add_out_file_record(c, path);
    }
    return record;
}

bool cache_read_in_file_record(Cache* c)
{
    char* path = NULL;
    if (!cache_read_str(c->log_file, c->allocator, &path)) return false;
    cache_merge_in_file_record(c, path);
    string_free(c->allocator, path);
    return true;
}

CacheRecordFile* cache_add_out_file_record(Cache* c, char const* path)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_OUT_FILE;
    cache_write_u32(f, type);
    cache_write_str(f, path);
    return cache_merge_out_file_record(c, path);
}

bool cache_read_out_file_record(Cache* c)
{
    char* path = NULL;
    if (!cache_read_str(c->log_file, c->allocator, &path)) return false;
    cache_merge_out_file_record(c, path);
    string_free(c->allocator, path);
    return true;
}

static void cache_write_file_array(Cache* c, CacheFile* files)
{
    uint32_t num_files = array_size(files);
    cache_write_u32(c->log_file, num_files);
    if (num_files)
    {
        fwrite(files, sizeof(CacheFile), num_files, c->log_file);
    }
}

void cache_write_cmd_record(Cache* c, CacheRecordCmd const* cmd)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_CMD;
    cache_write_u32(f, type);
    cache_write_str(f, cmd->name);
    cache_write_str(f, cmd->cmdline);
    cache_write_file_array(c, cmd->inputs);
    cache_write_file_array(c, cmd->outputs);
    cache_write_file_array(c, cmd->implicit_inputs);
    cache_merge_cmd_record(c, cmd);
}

static bool cache_read_file_array(Cache* c, CacheFile** out_files)
{
    FILE* f = c->log_file;
    uint32_t num_files = 0;
    if (!cache_read_u32(f, &num_files)) return false;
    CacheFile* files = NULL;
    if (num_files)
    {
        size_t num_bytes = sizeof(CacheFile) * num_files;
        if (num_bytes / sizeof(CacheFile) != num_files) return false;
        if (!cache_file_has_remaining(f, num_bytes)) return false;
        files = array_new(c->allocator, CacheFile, num_files, 0);
        if (!cache_read_bytes(f, files, num_bytes))
        {
            array_free(c->allocator, files);
            return false;
        }
    }
    *out_files = files;
    return true;
}

static bool cache_validate_file_array(Cache* c, CacheFile* files)
{
    size_t num_cached_files = array_size(c->strings);
    for (size_t i = 0; i != array_size(files); i++)
    {
        if (files[i].id >= num_cached_files) return false;
    }
    return true;
}

bool cache_read_cmd_record(Cache* c)
{
    FILE* f = c->log_file;
    CacheRecordCmd record = {0};
    if (!cache_read_str(f, c->allocator, &record.name)) return false;
    if (!cache_read_str(f, c->allocator, &record.cmdline)) return false;
    if (!cache_read_file_array(c, &record.inputs)) return false;
    if (!cache_read_file_array(c, &record.outputs)) return false;
    if (!cache_read_file_array(c, &record.implicit_inputs)) return false;
    if (!cache_validate_file_array(c, record.inputs)) return false;
    if (!cache_validate_file_array(c, record.outputs)) return false;
    if (!cache_validate_file_array(c, record.implicit_inputs)) return false;
    cache_merge_cmd_record(c, &record);
    string_free(c->allocator, record.name);
    string_free(c->allocator, record.cmdline);
    array_free(c->allocator, record.inputs);
    array_free(c->allocator, record.outputs);
    array_free(c->allocator, record.implicit_inputs);
    return true;
}

static void cache_write_string_array(Cache* c, char** strings)
{
    FILE* f = c->log_file;
    size_t num_string = array_size(strings);
    cache_write_u32(f, num_string);
    for (size_t i = 0; i != num_string; i++)
    {
        char const* string = strings[i];
        cache_write_str(f, string);
    }
}

static bool cache_read_string_array(Cache* c, char*** out_strings)
{
    uint32_t array_size = 0;
    if (!cache_read_u32(c->log_file, &array_size)) return false;
    if (array_size == 0)
    {
        *out_strings = NULL;
        return true;
    }
    size_t min_bytes = sizeof(uint32_t) * array_size;
    if (min_bytes / sizeof(uint32_t) != array_size) return false;
    if (!cache_file_has_remaining(c->log_file, min_bytes)) return false;
    char** strings = array_new(c->allocator, char*, array_size, 0);
    for (size_t i = 0; i != array_size; i++)
    {
        if (!cache_read_str(c->log_file, c->allocator, &strings[i]))
        {
            for (size_t j = 0; j != i; j++)
            {
                string_free(c->allocator, strings[j]);
            }
            array_free(c->allocator, strings);
            return false;
        }
    }
    *out_strings = strings;
    return true;
}

void cache_write_cpp_module_record(Cache* c, CacheRecordCppModule const* module)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_CPP_MODULE;
    cache_write_u32(f, type);
    fwrite(&module->source_id, sizeof(module->source_id), 1, f);
    cache_write_str(f, module->export);
    cache_write_string_array(c, module->imports);
    cache_merge_cpp_module_record(c, module);
}

bool cache_read_cpp_module_record(Cache* c)
{
    CacheRecordCppModule record = {0};
    if (!cache_read_bytes(c->log_file, &record.source_id, sizeof(record.source_id))) return false;
    if (record.source_id >= array_size(c->strings)) return false;
    if (!cache_read_str(c->log_file, c->allocator, &record.export)) return false;
    if (!cache_read_string_array(c, &record.imports)) return false;
    cache_merge_cpp_module_record(c, &record);
    array_free(c->allocator, record.export);
    for (size_t i = 0; i != array_size(record.imports); i++)
    {
        string_free(c->allocator, record.imports[i]);
    }
    array_free(c->allocator, record.imports);
    return true;
}

void cache_write_test_exe_record(Cache* c, CacheRecordTestExe const* exe)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_TEST_EXE;
    cache_write_u32(f, type);
    fwrite(&exe->source_id, sizeof(exe->source_id), 1, f);
    cache_write_string_array(c, exe->entries);
    cache_merge_test_exe_record(c, exe);
}

bool cache_read_test_exe_record(Cache* c)
{
    CacheRecordTestExe record = {0};
    if (!cache_read_bytes(c->log_file, &record.source_id, sizeof(record.source_id))) return false;
    if (record.source_id >= array_size(c->strings)) return false;
    if (!cache_read_string_array(c, &record.entries)) return false;
    cache_merge_test_exe_record(c, &record);
    for (size_t i = 0; i != array_size(record.entries); i++)
    {
        string_free(c->allocator, record.entries[i]);
    }
    array_free(c->allocator, record.entries);
    return true;
}

bool cache_read_records(Cache* c)
{
    uint32_t type = CACHE_RECORD_TYPE_EMPTY;
    while (true)
    {
        uint64_t remaining = 0;
        if (!cache_file_remaining(c->log_file, &remaining)) return false;
        if (remaining == 0)
        {
            return true;
        }
        if (remaining < sizeof(type)) return false;
        if (!cache_read_u32(c->log_file, &type)) return false;
        if (type == CACHE_RECORD_TYPE_EMPTY) return true;
        c->total_records_read++;
        bool b_ok = false;
        switch (type)
        {
        case CACHE_RECORD_TYPE_IN_FILE: b_ok = cache_read_in_file_record(c); break;
        case CACHE_RECORD_TYPE_OUT_FILE: b_ok = cache_read_out_file_record(c); break;
        case CACHE_RECORD_TYPE_CMD: b_ok = cache_read_cmd_record(c); break;
        case CACHE_RECORD_TYPE_CPP_MODULE: b_ok = cache_read_cpp_module_record(c); break;
        case CACHE_RECORD_TYPE_TEST_EXE: b_ok = cache_read_test_exe_record(c); break;
        default:
            error("Unknown cache record type: %d", type);
            return false;
        }
        if (!b_ok) return false;
    }
}

void cache_compact_log(Cache* c, char const* path)
{
    if (!c->b_needs_compaction) return;
    if (c->log_file)
    {
        fclose(c->log_file);
        c->log_file = NULL;
    }
    char const* tmp_path = string_from_print(allocator_temp(), "%s.tmp", path);
    FILE* tmpfile = os_fopen(tmp_path, "wb");
    cache_write_u32(tmpfile, CACHE_MAGIC);
    cache_write_u32(tmpfile, CACHE_VERSION);
    c->total_records_read = 0;

    Cache* temp_cache = cache_create(c->allocator);
    temp_cache->log_file = tmpfile;

    StringPtrHash* h = c->hash_name_to_cmd_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCmd* cmd = hash_value(h, i);
        for (size_t j = 0; j != array_size(cmd->inputs); j++)
        {
            uint32_t* id = &cmd->inputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_in_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        for (size_t j = 0; j != array_size(cmd->outputs); j++)
        {
            uint32_t* id = &cmd->outputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_out_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_out_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        for (size_t j = 0; j != array_size(cmd->implicit_inputs); j++)
        {
            uint32_t* id = &cmd->implicit_inputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_in_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        cache_write_cmd_record(temp_cache, cmd);
    }
    h = c->hash_source_path_to_cpp_module_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCppModule* cmd = hash_value(h, i);
        uint32_t* id = &cmd->source_id;
        char* path = cache_get_string(c, *id);
        CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
        if (!record)
        {
            record = cache_add_in_file_record(temp_cache, path);
        }
        *id = record->id;
        cache_write_cpp_module_record(temp_cache, cmd);
    }
    h = c->hash_source_path_to_test_exe_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordTestExe* cmd = hash_value(h, i);
        uint32_t* id = &cmd->source_id;
        char* path = cache_get_string(c, *id);
        CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
        if (!record)
        {
            record = cache_add_in_file_record(temp_cache, path);
        }
        *id = record->id;
        cache_write_test_exe_record(temp_cache, cmd);
    }
    cache_clear(c);
    *c = *temp_cache;
    allocator_free(c->allocator, temp_cache);

    fclose(tmpfile);
    os_rename(tmp_path, path);
    c->log_file = os_fopen(path, "ab");
    c->b_needs_compaction = false;
}

#include <stdbool.h>
#include <stdio.h>

extern Node** nodes;
extern Allocator* node_allocator;
extern ToolchainType self_build_toolchain;
extern ToolchainType default_toolchain;
extern OptimizationType default_optimization_type;
extern bool b_test_enabled;
extern bool b_node_default_excluded;

bool b_compdb = false;
bool b_dll_mode = false;
bool b_clean = false;
bool b_dry_run = false;
bool b_run_tests = false;
bool b_print_exe_entries = false;
bool b_generate_vscode_files = false;
bool b_bootstrap = false;
bool b_scan_deps_enabled = true;
bool b_content_hash = true;

char const* cl_show_include_prefix = "";
size_t max_jobs;
char** target_names;
char const* cup_h_dir = ".";
char const* vscode_debugger_type = NULL;
char const* init_cwd = NULL;
Node** targets = NULL;
Dylib* cup_dll = NULL;
FnAfterPrepare* fn_after_prepare = NULL;

static LockFileContext* process_lock_ctx;
size_t max_build_errors = 1;

void init_cache(void);
void destroy_var(void);
void save_last_status(void);

void destroy(void)
{
    extern Cache* cache;
    if (cache)
    {
        save_last_status();
        cache_compact_log(cache, fmt("{out_dir}/.cup_cache"));
        cache_destroy(cache);
        cache = NULL;
    }
    if (process_lock_ctx)
    {
        os_unlock_file(process_lock_ctx);
        process_lock_ctx = NULL;
    }
    if (node_allocator)
    {
        allocator_destroy(node_allocator);
        node_allocator = NULL;
    }
    if (init_cwd)
    {
        os_set_cwd(init_cwd);
        array_free(allocator_c(), init_cwd);
        init_cwd = NULL;
    }
    destroy_var();
    if (cup_dll)
    {
        dylib_unload(cup_dll);
        cup_dll = NULL;
    }
}

// To support multi-file selection compilation in Visual Studio
static char** objects_from_sources_string(char* sources_sep_with_semicolon, char const* cwd, char** objects, Allocator* allocator)
{
    size_t cwd_len = cwd ? strlen(cwd) + 1 : 0;
    char* p = strtok(sources_sep_with_semicolon, "\";");
    char const* out_dir = get_var("out_dir");
    for (char* token = p; token != NULL;)
    {
        char* path = string_from_print(allocator, "%s/obj/%s" OBJ_EXT, out_dir, token + cwd_len);
        array_push(allocator, objects, path);
        token = strtok(NULL, "\";");
    }
    return objects;
}

static void print_help(bool detailed)
{
    printf("Usage: cup [options] [targets]\n");
    printf("\nOptions:\n");
    printf("  -h                            Print short help\n");
    printf("  -hh                           Print detailed help\n");
    printf("  -out_dir <dir>                Set output directory\n");
    printf("  -t <toolchain>                Set toolchain (llvm, msvc, gcc, zig)\n");
    printf("  -linker <linker>              Set LLVM -fuse-ld linker (lld(==default), link, ld, default)\n");
    printf("  -O<level>                     Set optimization level (0, 3, s)\n");
    printf("  -clean                        Clean build\n");
    printf("  -dry                          Dry run (commands skipped but treated as success)\n");
    printf("  -test                         Run tests\n");
    printf("  -r, --bootstrap               Bootstrap (ignore build.c, build cup only)\n");
    printf("  -root <dir>                   Set root directory\n");
    if (detailed)
    {
        printf("\nDetailed Options:\n");
        printf("  -h\n");
        printf("        Print a short summary of available options.\n");
        printf("  -hh\n");
        printf("        Print this detailed help with option descriptions.\n");
        printf("  -out_dir <dir>\n");
        printf("        Specify the output directory for build artifacts.\n");
        printf("        Default: build/\n");
        printf("  -t <toolchain>\n");
        printf("        Select the toolchain to use for compilation.\n");
        printf("        Supported: llvm, msvc, gcc, zig\n");
        printf("  -linker <linker>\n");
        printf("        Select the linker to use with the LLVM toolchain via -fuse-ld=<linker>.\n");
        printf("        Supported: default, lld\n");
        printf("  -O<level>\n");
        printf("        0  : Debug (no optimization)\n");
        printf("        3  : Release (optimize for speed)\n");
        printf("        s  : Release (optimize for size)\n");
        printf("  -clean\n");
        printf("        Remove all build artifacts.\n");
        printf("  -dry\n");
        printf("        Commands are not executed but treated as successful.\n");
        printf("  -test\n");
        printf("        Build and run all test executables.\n");
        printf("  -r, --bootstrap\n");
        printf("        Ignore all custom build.c files and build cup itself only.\n");
        printf("  -root <dir>\n");
        printf("        Set the project root directory. All relative paths are resolved\n");
        printf("        relative to this directory.\n");
        printf("        Default: current working directory\n");
    }
}

static void parse_cmdline(void)
{
    void c_toolchain_set_llvm_linker_type_explicit(LinkerType type);

    char const* cli = os_get_cmdline();
    char const* p = cli;
    char* arg = NULL;
    Allocator* temp_allocator = allocator_temp();
    // skip first arg
    p = utilities_split_cmd(temp_allocator, p, &arg);
    Allocator* allocator = allocator_c();
    while (true)
    {
        p = utilities_split_cmd(temp_allocator, p, &arg);
        if (array_size(arg) == 0)
        {
            break;
        }
        else
        {
            // To support multi-file selection compilation in Visual Studio
            if (strcmp(arg, "-compile") == 0)
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                char const* cwd = get_var("workspace");
                target_names = objects_from_sources_string(arg, cwd, target_names, allocator);
                continue;
            }
            else if (string_equal(arg, "-root"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(true);
                    exit(EXIT_FAILURE);
                }
                set_root_dir(arg);
                continue;
            }
            else if (string_equal(arg, "-print_exe_entries"))
            {
                b_print_exe_entries = true;
                continue;
            }
            else if (string_equal(arg, "-clean"))
            {
                b_clean = true;
                continue;
            }
            else if (string_equal(arg, "-dry"))
            {
                b_dry_run = true;
                continue;
            }
            else if (string_equal(arg, "-t"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                if (string_equal(arg, "llvm")) set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
                else if (string_equal(arg, "msvc")) set_default_toolchain(TOOLCHAIN_TYPE_MSVC);
                else if (string_equal(arg, "gcc")) set_default_toolchain(TOOLCHAIN_TYPE_GCC);
                else if (string_equal(arg, "zig")) set_default_toolchain(TOOLCHAIN_TYPE_ZIG);
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-linker"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                if (string_equal(arg, "link")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LINK);
                else if (string_equal(arg, "ld")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LD);
                else if (string_equal(arg, "lld")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LLD);
                else if (string_equal(arg, "default")) c_toolchain_set_llvm_linker_type_explicit(LINKER_LLVM_LLD);
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-test"))
            {
                b_run_tests = true;
                set_test_enabled(true);
                continue;
            }
            else if (string_equal(arg, "-r") || string_equal(arg, "--bootstrap"))
            {
                b_bootstrap = true;
                continue;
            }
            else if (arg[0] == '-' && arg[1] == 'O' && arg[2] != '\0')
            {
                char const* level = arg + 2;
                if (string_equal(level, "0"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_DEBUG);
                }
                else if (string_equal(level, "3"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_RELEASE_FAST);
                }
                else if (string_equal(level, "s"))
                {
                    set_default_optimization(OPTIMIZATION_TYPE_RELEASE_SMALL);
                }
                else
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else if (string_equal(arg, "-out_dir"))
            {
                p = utilities_split_cmd(temp_allocator, p, &arg);
                if (array_size(arg) == 0)
                {
                    print_help(false);
                    exit(EXIT_FAILURE);
                }
                set_var("out_dir", arg);
                continue;
            }
            else if (string_equal(arg, "-h") || string_equal(arg, "-hh"))
            {
                print_help(string_equal(arg, "-hh"));
                exit(EXIT_SUCCESS);
            }
            else if (arg[0] == '-')
            {
                continue;
            }
            char* target = string_from_c_str(allocator, arg);
            array_push(allocator, target_names, target);
        }
    }
}

static void report(Node* cmd)
{
    int exit_code = cmd->exit_code;
    if (exit_code != EXIT_SUCCESS)
    {
        char const* desc = cmd_get_description(cmd);
        fprintf(stderr, "command failed: %s:%d:\n", cmd->file, cmd->line);
        if (array_size(desc))
        {
            fprintf(stderr, "%s\n", desc);
        }
    }
    else if (array_size(cmd->std_error) || array_size(cmd->std_output))
    {
        char const* cmdline = cmd_get_cmdline(cmd);
        if (cmdline)
        {
            fprintf(stderr, "%s\n", cmdline);
        }
        fprintf(stderr, "command output: %s:%d:\n", cmd->file, cmd->line);
    }
    if (array_size(cmd->std_error))
    {
        fputs(cmd->std_error, stderr);
        putc('\n', stderr);
    }
    if (array_size(cmd->std_output))
    {
        fputs(cmd->std_output, stdout);
        putc('\n', stdout);
    }
}

void restart(void)
{
    destroy();
    os_reset_env();
    // exit(0);
    char const* cmdline = os_get_cmdline();
    Process* new_process = os_start_process(cmdline);
    int exit_code = os_wait_process(new_process);
    os_forget_process(new_process);
    exit(exit_code);
}

char const* get_src_file_dir(char const* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_temp();
    char const* parent = path_parent_path(path, temp_allocator);
    char const* dir = path_lexically_normal(parent, allocator);
    return dir;
}

void determine_toolchain(void)
{
    ToolchainType c_toolchain_select_toolchain_automatically();
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_default_toolchain(c_toolchain_select_toolchain_automatically());
    }
    if (self_build_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
    {
        set_self_build_toolchain(default_toolchain);
    }
}

void sort_entries(void)
{
    Entry* entries = entry_get_all();
    if (entries)
    {
        qsort(entries, array_size(entries), sizeof(Entry), entry_compare);
    }
}

void invoke_entries()
{
    Entry* entries = entry_get_all();
    for (size_t i = 0; i != array_size(entries); i++)
    {
        allocator_reset_temp();
        char const* dir = get_src_file_dir(entries[i].file, allocator_temp());
        set_var("dir", dir);
        entries[i].fn();
        set_var("dir", NULL);
    }
}

static Node* wait_node(Executor* executor)
{
    Task* task = executor_wait(executor);
    if (task)
    {
        Node* node = executor_get_task_context(task);
        node->exit_code = executor_get_task_exit_code(task);
        executor_destroy_task(executor, task);
        return node;
    }
    return NULL;
}

extern char const* desc_color_error;
extern char const* desc_color_reset;

static int build(Graph* graph)
{
    int exit_code = EXIT_SUCCESS;
    size_t fail_count = 0;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    size_t num_jobs = max_jobs;
    if (num_jobs == 0)
    {
        num_jobs = os_get_cpu_count();
    }
    Executor* executor = executor_create(allocator, num_jobs);
    while (true)
    {
        allocator_reset_temp();
        Node* node = graph_pop(graph);
        if (node)
        {
            node->visit(node, graph, executor);
            if (!executor_is_full(executor)) continue;
        }
        else if (executor_is_empty(executor))
        {
            break;
        }
        node = wait_node(executor);
        if (node->exit_code == EXIT_SUCCESS)
        {
            node->after_execute(node);
            node->processed(node, graph);
        }
        else
        {
            exit_code = node->exit_code;
            fail_count++;
        }
        report(node);
        if (max_build_errors && fail_count >= max_build_errors)
        {
            break;
        }
    }

    Node** unreachable_nodes = graph_get_unreachable_nodes(graph, allocator);
    if (array_size(unreachable_nodes) && exit_code == EXIT_SUCCESS)
    {
        for (size_t i = 0; i != array_size(unreachable_nodes); i++)
        {
            Node* n = unreachable_nodes[i];
            fprintf(stderr, "%serror%s: unreachable node: %s\n", desc_color_error, desc_color_reset, n->path);
        }
    }
    executor_destroy(executor);
    allocator_destroy(allocator);
    return exit_code;
}

static int determine_build_targets(void)
{
    array_resize(allocator_c(), targets, 0);
    for (size_t i = 0; i != array_size(target_names); i++)
    {
        char const* name = target_names[i];
        Node* target = find_node(name);
        if (target)
        {
            array_push(allocator_c(), targets, target);
        }
        else
        {
            warn("Unknown target: %s", name);
            return EXIT_FAILURE;
        }
    }
    if (array_size(targets) == 0)
    {
        Node** nodes = get_all_nodes();
        for (size_t i = 0; i != array_size(nodes); i++)
        {
            Node* node = nodes[i];
            if (!node->b_default_excluded)
            {
                array_push(allocator_c(), targets, node);
            }
        }
    }
    return EXIT_SUCCESS;
}

static Node* create_run_test_target(Node* exe)
{
    Node* run_test_cmd = CMD_FROM_EXE(exe, fmt("run test: {:n}", exe));
    run_test_cmd->b_dirty = true;
    array_push(node_allocator, targets, run_test_cmd);
    node_ensure_prepared(run_test_cmd);
    return run_test_cmd;
}

static int determine_test_targets(void)
{
    uint32_t test_type = node_make_file_type(FILE_TYPE_EXE, C_FILE_TEST);
    array_resize(node_allocator, targets, 0);
    for (size_t i = 0; i != array_size(target_names); i++)
    {
        char const* name = target_names[i];
        Node* node = find_node(name);
        if (node)
        {
            if (node->type != test_type)
            {
                warn("Unknown test target: skip %s", name);
                continue;
            }
            create_run_test_target(node);
        }
        else
        {
            warn("Unknown target: %s", name);
            return EXIT_FAILURE;
        }
    }
    if (array_size(targets) == 0)
    {
        for (size_t i = 0; i != array_size(nodes); i++)
        {
            Node* node = nodes[i];
            if (node->type != test_type)
            {
                continue;
            }
            create_run_test_target(node);
        }
    }
    return EXIT_SUCCESS;
}

static void remove_generated_files(Node* target, Set* set, Node* self)
{
    bool b_existed;
    hash_insert_check(set, (uintptr_t)target, &b_existed);
    if (b_existed)
    {
        return;
    }
    Node* dll = NULL;
    if (b_dll_mode)
    {
        dll = DLL("{out_dir}/{self_name}");
    }
    if (target->node_type == NODE_TYPE_FILE && target != self)
    {
        if (target->build_cmd)
        {
            if (os_file_exists(target->path))
            {
                printf("Removing file: %s\n", target->path);
                if (!b_dry_run)
                {
                    if (target == dll)
                    {
                        void c_toolchain_rename_to_old(char const* path);
                        c_toolchain_rename_to_old(target->path);
                    }
                    else
                    {
                        os_remove_file(target->path);
                    }
                }
            }
        }
    }
    for (size_t j = 0; j != array_size(target->dependencies); j++)
    {
        remove_generated_files(target->dependencies[j], set, self);
    }
}

static void clean(void)
{
    Cache* cache = cache_load_readonly(allocator_temp(), fmt("{out_dir}/.cup_cache"));
    if (!cache)
    {
        return;
    }
    char const* dll_path = NULL;
    if (b_dll_mode)
    {
        dll_path = fmt("{out_dir}/{self_name}" DLL_EXT);
    }
    Node* self = get_or_add_file_with_type(fmt("{self}"), FILE_TYPE_EXE);
    StringPtrHash* h = cache->hash_path_to_output_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        char const* path = cache_get_string(cache, record->id);
        if (string_equal(self->path, path) || string_equal(path, "build.c"))
        {
            continue;
        }
        if (os_file_exists(path))
        {
            printf("Removing file: %s\n", path);
            if (!b_dry_run)
            {
                if (dll_path && string_equal(dll_path, path))
                {
                    void c_toolchain_rename_to_old(char const* path);
                    c_toolchain_rename_to_old(path);
                }
                else
                {
                    os_remove_file(path);
                }
            }
        }
    }
}

static Node* get_self(void)
{
    if (b_dll_mode)
    {
        return DLL("{out_dir}/{self_name}");
    }
    else
    {
        return EXE("{self_name}");
    }
}

void get_scan_test_cmds(Allocator* allocator, Node*** out_cmds);

static int scan_tests(void)
{
    int exit_code = EXIT_SUCCESS;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node** targets = NULL;
    if (b_test_enabled)
    {
        get_scan_test_cmds(allocator, &targets);
    }
    if (array_size(targets))
    {
        Graph* graph = graph_create(allocator, targets, array_size(targets));
        exit_code = build(graph);
    }
    allocator_destroy(allocator);
    return exit_code;
}

static void add_test_executables(void)
{
    Node* add_test_exe_for_obj(Node * obj, char const** entries);

    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_CMD ||
            node->cmd_type != CMD_TYPE_EXECUTABLE ||
            node->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        CCompileCmd* cc = (CCompileCmd*)node;
        if (array_size(cc->src->test_entries))
        {
            add_test_exe_for_obj(cc->out_obj, cc->src->test_entries);
        }
    }
}

static int scan_deps(void)
{
    if (!b_scan_deps_enabled)
    {
        return 0;
    }
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE);
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->type != type)
        {
            continue;
        }
        CCompileCmd* cmd = (CCompileCmd*)node;
        if (cmd->b_cpp &&
            cmd->cpp_std >= CPP_LANGUAGE_STANDARD_20 &&
            cmd->export_name == NULL &&
            array_size(cmd->import_names) == 0)
        {
            cmd->scan_deps_cmd = (ScanDepsCmd*)scan_deps_cmd_create(cmd);
            cmd->scan_deps_cmd->b_default_excluded = true;
        }
    }
    int exit_code = EXIT_SUCCESS;
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    extern Node** scan_deps_cmds;
    if (array_size(scan_deps_cmds))
    {
        Graph* graph = graph_create(allocator, scan_deps_cmds, array_size(scan_deps_cmds));
        exit_code = build(graph);
    }
    allocator_destroy(allocator);
    for (size_t i = 0; i != array_size(scan_deps_cmds); i++)
    {
        ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmds[i];
        CCompileCmd* cc = scan->compile_cmd;
        if (scan->export_name)
        {
            Node* bmi = module_from_src(cc->src);
            c_compile_cmd_set_export((Node*)cc, scan->export_name, bmi);
        }
        if (cc->export_name)
        {
            Node* bmi = module_from_src(cc->src);
            c_compile_cmd_set_export((Node*)cc, cc->export_name, bmi);
        }
    }
    for (size_t i = 0; i != array_size(scan_deps_cmds); i++)
    {
        ScanDepsCmd* scan = (ScanDepsCmd*)scan_deps_cmds[i];
        CCompileCmd* cc = scan->compile_cmd;
        for (size_t j = 0; j != array_size(scan->imports); j++)
        {
            char const* import = scan->imports[j];
            Node* bmi = hash_get(cc->import_map, import);
            c_compile_cmd_add_import((Node*)cc, import, bmi);
        }
    }
    return exit_code;
}

static void prepare(void)
{
    for (size_t start = 0, num_nodes = array_size(nodes); start != num_nodes;)
    {
        for (size_t i = start; i != num_nodes; i++)
        {
            node_ensure_prepared(nodes[i]);
        }
        start = num_nodes;
        num_nodes = array_size(nodes);
    };
}

typedef struct VSCodeLaunchEntry
{
    char const* name;
    char const* debugger_type;
    char const* program;
    char const* group;
    char const** args;
    size_t num_args;
} VSCodeLaunchEntry;

static char* print_vscode_launch_entry(char* str, VSCodeLaunchEntry const* entry, Allocator* allocator)
{
    string_printf(allocator, str, "        {\n");
    string_printf(allocator, str, "            \"name\": \"%s\",\n", entry->name);
    string_printf(allocator, str, "            \"type\": \"%s\",\n", entry->debugger_type);
    string_printf(allocator, str, "            \"request\": \"%s\",\n", "launch");
    string_printf(allocator, str, "            \"program\": \"${workspaceFolder}/%s\",\n", entry->program);
    if (!string_equal(entry->debugger_type, "cppdbg"))
    {
        string_printf(allocator, str, "            \"console\": \"integratedTerminal\",\n");
    }
    string_printf(allocator, str, "            \"cwd\": \"${workspaceFolder}\"");
    if (entry->group)
    {
        string_printf(allocator, str, ",\n");
        string_printf(allocator, str, "            \"presentation\": {\n");
        string_printf(allocator, str, "                \"group\": \"%s\"\n", entry->group);
        string_printf(allocator, str, "            }");
    }
    if (entry->args)
    {
        bool b_first = true;
        string_printf(allocator, str, ",\n");
        string_printf(allocator, str, "            \"args\": [");
        for (size_t i = 0; i != entry->num_args; i++)
        {
            char const* arg = entry->args[i];
            if (b_first)
            {
                b_first = false;
            }
            else
            {
                string_printf(allocator, str, ",");
            }
            string_printf(allocator, str, "\"%s\"", arg);
        }
        string_printf(allocator, str, "]");
    }
    string_printf(allocator, str, "\n");
    string_printf(allocator, str, "        }");
    return str;
}

static char const* get_toolchain_vscode_debugger_type(ToolchainType toolchain)
{
    if (vscode_debugger_type == NULL)
    {
        if (CURRENT_PLATFORM != PLATFORM_WINDOWS || toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_TCC)
        {
            return "cppdbg";
        }
        return "cppvsdbg";
    }
    return vscode_debugger_type;
}

static char const* get_exe_vscode_debugger_type(Node* exe)
{
    if (exe->build_cmd && exe->build_cmd->cmd_ext_type == C_CMD_LINK)
    {
        LinkCmd* link = (LinkCmd*)exe->build_cmd;
        return get_toolchain_vscode_debugger_type(link->toolchain);
    }
    return get_toolchain_vscode_debugger_type(default_toolchain);
}

static char* print_vscode_launch_configurations(char* str, Allocator* allocator)
{
    string_printf(allocator, str, "[\n");
    bool b_first = true;
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->file_type != FILE_TYPE_EXE)
        {
            continue;
        }
        VSCodeLaunchEntry entry = {.program = node->path, .debugger_type = get_exe_vscode_debugger_type(node)};
        bool b_test = is_test_exe(node);
        if (b_test)
        {
            entry.group = node->path;
            char const** entries = get_test_entries(node);
            for (size_t j = 0; j != array_size(entries); j++)
            {
                entry.name = string_from_print(allocator_temp(), "%s %s", node->path, entries[j]);
                entry.args = &entries[j];
                entry.num_args = 1;
                if (b_first)
                {
                    b_first = false;
                }
                else
                {
                    string_printf(allocator, str, ",\n");
                }
                str = print_vscode_launch_entry(str, &entry, allocator);
            }
        }
        entry.name = node->path;
        entry.args = node->debugger_run_arguments;
        entry.num_args = array_size(node->debugger_run_arguments);
        if (b_first)
        {
            b_first = false;
        }
        else
        {
            string_printf(allocator, str, ",\n");
        }
        str = print_vscode_launch_entry(str, &entry, allocator);
    }
    if (!b_first)
    {
        string_printf(allocator, str, "\n");
    }
    string_printf(allocator, str, "    ]\n");
    return str;
}

static char* gen_vscode_launch_json_string(Allocator* allocator)
{
    char* result = NULL;
    string_printf(allocator, result, "{\n");
    string_printf(allocator, result, "    \"version\": \"0.2.0\",\n");
    string_printf(allocator, result, "    \"configurations\": ");
    result = print_vscode_launch_configurations(result, allocator);
    string_printf(allocator, result, "}");
    return result;
}

static int gen_vscode_launch_json_callback(Node* cmd)
{
    char* launch_json = cmd->extra_data;
    if (!launch_json)
    {
        launch_json = gen_vscode_launch_json_string(allocator_c());
    }
    char const* path = cmd->outputs[0]->path;
    os_write_all(path, launch_json, array_size(launch_json));
    array_free(allocator_c(), launch_json);
    return EXIT_SUCCESS;
}

static bool gen_vscode_launch_json_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    Allocator* temp_allocator = allocator_create_chained();
    char* new_launch_json = gen_vscode_launch_json_string(allocator_c());
    char const* path = cmd->outputs[0]->path;
    char* old_content = os_read_all(temp_allocator, path);
    bool b_dirty = false;
    if (old_content == NULL || !string_equal(old_content, new_launch_json))
    {
        cmd->extra_data = new_launch_json;
        b_dirty = true;
    }
    else
    {
        array_free(allocator_c(), new_launch_json);
    }
    allocator_destroy(temp_allocator);
    return b_dirty;
}

void set_generate_vscode_files_enabled(bool enabled)
{
    b_generate_vscode_files = enabled;
}

void set_vscode_debugger_type(char const* type)
{
    vscode_debugger_type = type;
}

void set_after_prepare_callback(FnAfterPrepare* fn)
{
    fn_after_prepare = fn;
}

ENTRY(gen_vscode_launch_json)
{
    Node* output = get_or_add_file(".vscode/launch.json");
    Node* cmd = CALLBACK_CMD(gen_vscode_launch_json_callback, output->path);
    node_set_alias(output, "vsc_launch");
    node_set_check_dirty_fn(cmd, gen_vscode_launch_json_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));

    output->b_default_excluded = !b_generate_vscode_files;
    cmd->b_default_excluded = !b_generate_vscode_files;
}

static char* gen_vscode_tasks_json_string(Allocator* allocator)
{
    char* result = NULL;
    string_printf(allocator, result, "{\n");
    string_printf(allocator, result, "    \"version\": \"2.0.0\",\n");
    string_printf(allocator, result, "    \"tasks\": [");
    char const* self_name = get_var("self_name");
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->build_cmd == NULL)
        {
            continue;
        }
        string_printf(allocator, result, "\n");
        string_printf(allocator, result, "        {\n");
        string_printf(allocator, result, "            \"label\": \"%s\",\n", node->path);
        string_printf(allocator, result, "            \"type\": \"process\",\n");
        string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
        string_printf(allocator, result, "            \"args\": [\"%s\"],\n", node->path);
        string_printf(allocator, result, "            \"group\": {\"kind\": \"build\"}\n");
        string_printf(allocator, result, "        },");
    }
    // All
    string_printf(allocator, result, "\n");
    string_printf(allocator, result, "        {\n");
    string_printf(allocator, result, "            \"label\": \"%s\",\n", "All");
    string_printf(allocator, result, "            \"type\": \"process\",\n");
    string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
    string_printf(allocator, result, "            \"group\": {\"kind\": \"build\", \"isDefault\": true}\n");
    string_printf(allocator, result, "        },");
    // Clean
    string_printf(allocator, result, "\n");
    string_printf(allocator, result, "        {\n");
    string_printf(allocator, result, "            \"label\": \"%s\",\n", "Clean");
    string_printf(allocator, result, "            \"type\": \"process\",\n");
    string_printf(allocator, result, "            \"command\": \"%s%s\",\n", self_name, EXE_EXT);
    string_printf(allocator, result, "            \"args\": [\"clean\"],\n");
    string_printf(allocator, result, "            \"group\": {\"kind\": \"build\"}\n");
    string_printf(allocator, result, "        }");
    string_printf(allocator, result, "\n");

    string_printf(allocator, result, "    ]\n");
    string_printf(allocator, result, "}");
    return result;
}

static bool gen_vscode_tasks_json_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    char* new_content = gen_vscode_tasks_json_string(allocator_c());
    char const* path = cmd->outputs[0]->path;
    Allocator* allocator = allocator_create_chained();
    char const* old = os_read_all(allocator, path);
    bool b_dirty = false;
    if (old == NULL || !string_equal(new_content, old))
    {
        b_dirty = true;
        cmd->extra_data = new_content;
    }
    else
    {
        array_free(allocator_c(), new_content);
    }
    allocator_destroy(allocator);
    return b_dirty;
}

static int gen_vscode_tasks_json_callback(Node* cmd)
{
    char* content = cmd->extra_data;
    if (!content)
    {
        content = gen_vscode_tasks_json_string(allocator_c());
    }
    char const* path = cmd->outputs[0]->path;
    os_write_all(path, content, array_size(content));
    array_free(allocator_c(), content);
    return EXIT_SUCCESS;
}

ENTRY(gen_vscode_tasks_json)
{
    Node* output = get_or_add_file(".vscode/tasks.json");
    output->b_default_excluded = !b_generate_vscode_files;
    Node* cmd = CALLBACK_CMD(gen_vscode_tasks_json_callback, NULL);
    cmd->b_default_excluded = !b_generate_vscode_files;
    node_set_alias(output, "vsc_tasks");
    node_set_check_dirty_fn(cmd, gen_vscode_tasks_json_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));
}

static int print_exe_entries(void)
{
    Allocator* temp_allocator = allocator_temp();
    char* configurations = string_from_c_str(temp_allocator, "    ");
    configurations = print_vscode_launch_configurations(configurations, temp_allocator);
    puts(configurations);
    return 0;
}

void collect_build_scripts(char const* directory, Allocator* allocator)
{
    extern StringSet* build_scripts;
    Allocator* temp_allocator = allocator_temp();
    Directory* d = directory_open(directory, temp_allocator);
    if (d)
    {
        while (true)
        {
            DirectoryEntry* entry = directory_read(d);
            if (!entry)
            {
                directory_close(d);
                break;
            }
            if (string_equal(entry->name, ".") || string_equal(entry->name, ".."))
            {
                continue;
            }
            if (entry->is_directory)
            {
                char const* sub_dir = string_from_print(temp_allocator, "%s/%s", directory, entry->name);
                collect_build_scripts(sub_dir, allocator);
            }
            else
            {
                if (string_equal(entry->name, "build.c"))
                {
                    bool b_existed;
                    char* path = string_from_print(allocator, "%s/%s", directory, entry->name);
                    hash_insert_check(build_scripts, path, &b_existed);
                    if (b_existed)
                    {
                        array_free(allocator, path);
                    }
                }
            }
        }
    }
}

void set_cup_h_dir(char const* dir)
{
    cup_h_dir = dir;
}

void set_node_default_excluded(bool b_default_excluded)
{
    b_node_default_excluded = b_default_excluded;
}

void set_content_hash_enabled(bool b_enabled)
{
    b_content_hash = b_enabled;
}

static Node* add_copy_cmd_windows(Node* input, Node* output, char const* file, int line)
{
    char* src = string_new(allocator_temp(), array_size(input->path), input->path);
    char* dst = string_new(allocator_temp(), array_size(output->path), output->path);
    path_slash_to_backslash(src);
    path_slash_to_backslash(dst);
    if (file_path_has_space(input))
    {
        src = string_from_print(allocator_temp(), "\"%s\"", src);
    }
    if (file_path_has_space(output))
    {
        dst = string_from_print(allocator_temp(), "\"%s\"", dst);
    }
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    char const* name = fmt("copy: output: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    cmd_add_option(node, NULL, "cmd /c copy", OPTION_EXE);
    cmd_add_option(node, NULL, src, OPTION_INPUT);
    cmd_add_option(node, NULL, dst, OPTION_OUTPUT);
    cmd_add_option(node, "/Y", NULL, OPTION_FLAG);
    cmd_add_option(node, "> nul", NULL, OPTION_FLAG);
    cmd_add_input(node, input);
    cmd_add_output(node, output);
    return node;
}

static Node* add_copy_cmd_linux(Node* input, Node* output, char const* file, int line)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    char const* name = fmt("copy: output: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    cmd_add_option(node, NULL, "cp", OPTION_EXE);
    cmd_add_input_file_option(node, NULL, input);
    cmd_add_output_file_option(node, NULL, output);
    return node;
}

Node* add_copy_cmd(Node* input, Node* output, char const* file, int line)
{
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return add_copy_cmd_windows(input, output, file, line);
    }
    return add_copy_cmd_linux(input, output, file, line);
}

ArchitectureType get_self_build_arch(void)
{
    return CURRENT_ARCHITECTURE;
}

int build_self(void)
{
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node* self = get_self();
    Graph* graph = graph_create(allocator, &self, 1);
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static int build_targets(void)
{
    determine_build_targets();
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static void cmd_visit_dry_run(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    char const* desc = cmd_get_description(node);
    if (array_size(desc))
    {
        puts(desc);
    }
    node->processed(node, graph);
}

static int dry_run(void)
{
    Allocator* allocator = allocator_create_tiny(4096, 4096 * 64);
    Node** targets = get_all_nodes();
    for (size_t i = 0; i != array_size(targets); i++)
    {
        Node* node = targets[i];
        if (node->node_type != NODE_TYPE_CMD)
        {
            continue;
        }
        Cmd* cmd = (Cmd*)node;
        cmd->visit = cmd_visit_dry_run;
    }
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    int exit_code = build(graph);
    allocator_destroy(allocator);
    return exit_code;
}

static int run_tests()
{
    build_self();
    determine_test_targets();
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, targets, array_size(targets));
    max_build_errors = array_size(targets);
    int exit_code = build(graph);
    max_build_errors = 1;
    size_t total = 0;
    size_t passed = 0;
    for (size_t i = 0; i != array_size(targets); i++)
    {
        total++;
        if (targets[i]->exit_code == EXIT_SUCCESS)
        {
            passed++;
        }
    }
    if (total)
    {
        fprintf(stdout, "\nresults: %zu total, %zu passed, %zu failed\n", total, passed, total - passed);
    }
    allocator_destroy(allocator);
    return exit_code;
}

void init_mode(void);
void init_toolchain(void);
bool c_toolchain_is_linker_type_explicit(void);
void c_toolchain_restore_llvm_linker_type(LinkerType type);

static void read_last_status(void)
{
    if (default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED ||
        default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED ||
        !c_toolchain_is_linker_type_explicit())
    {
        char const* status_path = fmt("{out_dir}/.last_status");
        char* content = os_read_all(allocator_temp(), status_path);
        if (content)
        {
            char* line = strtok(content, "\r\n");
            if (line && default_toolchain == TOOLCHAIN_TYPE_UNSPECIFIED)
            {
                if (string_equal(line, "llvm")) set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
                else if (string_equal(line, "msvc")) set_default_toolchain(TOOLCHAIN_TYPE_MSVC);
                else if (string_equal(line, "gcc")) set_default_toolchain(TOOLCHAIN_TYPE_GCC);
                else if (string_equal(line, "zig")) set_default_toolchain(TOOLCHAIN_TYPE_ZIG);
                else if (string_equal(line, "tcc")) set_default_toolchain(TOOLCHAIN_TYPE_TCC);
            }
            line = strtok(NULL, "\r\n");
            if (line && default_optimization_type == OPTIMIZATION_TYPE_UNSPECIFIED)
            {
                if (string_equal(line, "debug")) set_default_optimization(OPTIMIZATION_TYPE_DEBUG);
                else if (string_equal(line, "release_fast")) set_default_optimization(OPTIMIZATION_TYPE_RELEASE_FAST);
                else if (string_equal(line, "release_small")) set_default_optimization(OPTIMIZATION_TYPE_RELEASE_SMALL);
            }
            line = strtok(NULL, "\r\n");
            if (line && !c_toolchain_is_linker_type_explicit() && default_toolchain == TOOLCHAIN_TYPE_LLVM)
            {
                if (string_equal(line, "link")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LINK);
                else if (string_equal(line, "ld")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LD);
                else if (string_equal(line, "lld")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LLD);
                else if (string_equal(line, "default")) c_toolchain_restore_llvm_linker_type(LINKER_LLVM_LLD);
            }
        }
    }
}

void save_last_status(void)
{
    char const* out_dir = get_var("out_dir");
    os_create_directory_tree(out_dir);
    char const* status_path = fmt("{out_dir}/.last_status");
    char const* tc_str = NULL;
    switch (default_toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC: tc_str = "msvc"; break;
    case TOOLCHAIN_TYPE_LLVM: tc_str = "llvm"; break;
    case TOOLCHAIN_TYPE_ZIG: tc_str = "zig"; break;
    case TOOLCHAIN_TYPE_GCC: tc_str = "gcc"; break;
    case TOOLCHAIN_TYPE_TCC: tc_str = "tcc"; break;
    default: tc_str = "unspecified"; break;
    }
    char const* opt_str = NULL;
    switch (default_optimization_type)
    {
    case OPTIMIZATION_TYPE_DEBUG: opt_str = "debug"; break;
    case OPTIMIZATION_TYPE_RELEASE_FAST: opt_str = "release_fast"; break;
    case OPTIMIZATION_TYPE_RELEASE_SMALL: opt_str = "release_small"; break;
    default: opt_str = "release"; break;
    }
    char const* linker_str = "";
    if (default_toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        switch (get_llvm_linker_type())
        {
        case LINKER_LLVM_LD: linker_str = "ld"; break;
        case LINKER_LLVM_LLD: linker_str = "lld"; break;
        case LINKER_LLVM_LINK: linker_str = "link"; break;
        default: linker_str = "default"; break;
        }
    }
    char* content = string_from_print(allocator_temp(), "%s\n%s\n%s\n", tc_str, opt_str, linker_str);
    os_write_all(status_path, content, array_size(content));
}

void set_root_dir(char const* dir)
{
    extern void var_on_cwd_changed(void);

    if (!os_file_exists(dir))
    {
        error("root directory not found");
        exit(EXIT_FAILURE);
    }
    os_set_cwd(dir);
    var_on_cwd_changed();
}

CONSTRUCTOR(init)
static void init(void)
{
    os_set_console_utf8();
    init_cwd = os_get_cwd(allocator_c());
    init_var();
    parse_cmdline();
    read_last_status();
    init_node();
    init_toolchain();
    if (atexit(destroy) != 0)
    {
        printf("atexit failed!\n");
        exit(EXIT_FAILURE);
    }
    init_mode();
}

int execute(void)
{
    determine_toolchain();
    sort_entries();
    char const* lock_path = fmt("{out_dir}/.cup_lock");
    process_lock_ctx = os_lock_file(lock_path, allocator_c(), false);
    if (b_clean)
    {
        clean();
        return 0;
    }
    init_cache();
    if (b_clean)
    {
        clean();
        return 0;
    }
    invoke_entries();
    int exit_code;
    if (b_test_enabled)
    {
        exit_code = scan_tests();
        add_test_executables();
        if (exit_code == EXIT_FAILURE)
        {
            return exit_code;
        }
    }
    if (b_scan_deps_enabled)
    {
        exit_code = scan_deps();
        if (exit_code != EXIT_SUCCESS)
        {
            return exit_code;
        }
    }
    prepare();
    if (fn_after_prepare)
    {
        fn_after_prepare();
    }
    if (b_print_exe_entries)
    {
        return print_exe_entries();
    }
    if (b_dry_run)
    {
        return dry_run();
    }
    if (b_run_tests)
    {
        exit_code = run_tests();
        return exit_code;
    }
    exit_code = build_self();
    if (exit_code != EXIT_SUCCESS)
    {
        return exit_code;
    }
    exit_code = build_targets();
    return exit_code;
}

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void depfile_eat_chars(FILE* file, size_t n)
{
    fseek(file, (long)n, SEEK_CUR);
}

static void depfile_peek_chars(FILE* file, size_t n, char* out)
{
    long pos = ftell(file);
    size_t read = fread(out, 1, n, file);
    if (read != n)
    {
        memset(out + read, EOF, n - read);
    }
    fseek(file, pos, SEEK_SET);
}

static void depfile_skip_spaces_and_continuations(FILE* f)
{
    for (;;)
    {
        char ch[2];
        depfile_peek_chars(f, 2, ch);

        if (ch[0] == ' ' || ch[0] == '\t')
        {
            depfile_eat_chars(f, 1);
            continue;
        }

        if (ch[0] == '\\' && (ch[1] == '\r' || ch[1] == '\n'))
        {
            depfile_eat_chars(f, 1);
            depfile_peek_chars(f, 1, ch);
            if (ch[0] == '\r') depfile_eat_chars(f, 1);
            depfile_peek_chars(f, 1, ch);
            if (ch[0] == '\n') depfile_eat_chars(f, 1);
            continue;
        }

        break;
    }
}

static bool is_variable_assignment_line(FILE* f)
{
    long original_pos = ftell(f);
    bool is_assignment = false;
    char ch;

    while (fread(&ch, 1, 1, f) == 1)
    {
        if (ch == '\n' || ch == '\r') break;
        if (ch == ':') break;
        if (ch == '=')
        {
            is_assignment = true;
            break;
        }
    }

    fseek(f, original_pos, SEEK_SET);
    return is_assignment;
}

static void depfile_skip_current_line(FILE* f)
{
    char ch;
    while (fread(&ch, 1, 1, f) == 1)
    {
        if (ch == '\n') break;
    }
}

void depfile_parser_init(DepfileParser* parser, FILE* f)
{
    parser->file = f;
    parser->state = 0;
    parser->is_phony_rule = false;
}

bool depfile_parser_next(DepfileParser* p, Allocator* allocator, char** out_path, DepfileItemType* out_type)
{
    for (;;)
    {
        if (p->state == 0)
        {
            depfile_skip_spaces_and_continuations(p->file);
            char peek_eof;
            long pos = ftell(p->file);
            if (fread(&peek_eof, 1, 1, p->file) == 0) return false;
            fseek(p->file, pos, SEEK_SET);
            if (is_variable_assignment_line(p->file))
            {
                depfile_skip_current_line(p->file);
                continue;
            }
        }
        depfile_skip_spaces_and_continuations(p->file);

        char ch[2];
        depfile_peek_chars(p->file, 2, ch);

        if (ch[0] == EOF)
        {
            return false;
        }

        if (ch[0] == ':')
        {
            depfile_eat_chars(p->file, 1);
            p->state = 1;
            continue;
        }

        if (ch[0] == '|')
        {
            depfile_eat_chars(p->file, 1);
            p->state = 2;
            continue;
        }

        if (ch[0] == '\n' || ch[0] == '\r')
        {
            if (ch[0] == '\r') depfile_eat_chars(p->file, 1);
            depfile_peek_chars(p->file, 1, ch);
            if (ch[0] == '\n') depfile_eat_chars(p->file, 1);

            p->state = 0;
            p->is_phony_rule = false;
            continue;
        }

        bool word_read = false;
        for (;;)
        {
            char next[2];
            depfile_peek_chars(p->file, 2, next);

            if (next[0] == (char)EOF || isspace(next[0]) || next[0] == '|')
            {
                break;
            }

            if (next[0] == ':')
            {
                if (next[1] == '/' || next[1] == '\\')
                {
                    array_push(allocator, *out_path, ':');
                    depfile_eat_chars(p->file, 1);
                    word_read = true;
                    continue;
                }
                else
                {
                    break;
                }
            }
            if (next[0] == '\\')
            {
                if (next[1] == ' ')
                {
                    depfile_eat_chars(p->file, 2);
                    array_push(allocator, *out_path, ' ');
                    word_read = true;
                    continue;
                }
                if (next[1] == '\r' || next[1] == '\n')
                {
                    break;
                }

                depfile_eat_chars(p->file, 1);
                array_push(allocator, *out_path, '/');
                word_read = true;
                continue;
            }

            array_push(allocator, *out_path, next[0]);
            depfile_eat_chars(p->file, 1);
            word_read = true;
        }

        if (word_read)
        {
            array_push(allocator, *out_path, 0);
            array_pop(*out_path);

            if (p->state == 0)
            {
                *out_type = DEPFILE_ITEM_TARGET;

                if (strcmp(*out_path, ".PHONY") == 0)
                {
                    p->is_phony_rule = true;
                }
            }
            else if (p->state == 1)
            {
                *out_type = p->is_phony_rule ? DEPFILE_ITEM_PHONY : DEPFILE_ITEM_NORMAL_DEP;
            }
            else
            {
                *out_type = DEPFILE_ITEM_ORDER_ONLY_DEP;
            }

            return true;
        }
    }
}

static Entry* entries = NULL;

Entry* entry_get_all(void)
{
    return entries;
}

void entry_push(Entry entry)
{
    array_push(allocator_c(), entries, entry);
}

void entry_clean()
{
    array_free(allocator_c(), entries);
}

#if CURRENT_PLATFORM == PLATFORM_WINDOWS
#elif CURRENT_PLATFORM == PLATFORM_LINUX
#elif CURRENT_PLATFORM == PLATFORM_MACOS
#else
#error "unknown platform"
#endif

#include <assert.h>

void executor_platform_init(Executor* executor);
void executor_platform_destroy(Executor* executor);
void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task);

Executor* executor_create(Allocator* allocator, size_t num_slots)
{
    Executor* executor = allocator_calloc(allocator, 1, sizeof(Executor));
    executor->allocator = allocator;
    executor->slots = allocator_calloc(allocator, num_slots, sizeof(ExecutorSlot));
    executor->num_slots = num_slots;
    executor->num_running_tasks = 0;
    executor->pending_tasks = NULL;
    executor_platform_init(executor);
    return executor;
}

void executor_destroy(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task)
        {
            executor_force_kill_task(slot);
        }
    }
    executor_platform_destroy(executor);
    allocator_free(executor->allocator, executor->slots);
    allocator_free(executor->allocator, executor);
}

bool executor_is_full(Executor* executor)
{
    return executor->num_running_tasks == executor->num_slots;
}

bool executor_is_empty(Executor* executor)
{
    return executor->num_running_tasks == 0 &&
           array_size(executor->pending_tasks) == 0;
}

uint32_t executor_find_empty_slot(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task == NULL)
        {
            return i + 1;
        }
    }
    return 0;
}

uint32_t executor_find_task_slot_id(Executor* executor, Task* task)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        if (executor->slots[i].task == task)
        {
            return i + 1;
        }
    }
    return 0;
}

uint32_t executor_find_finished_slot(Executor* executor)
{
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        if (slot->task == NULL)
        {
            continue;
        }
        if (slot->b_finished)
        {
            return i + 1;
        }
    }
    return 0;
}

ExecutorSlot* executor_get_slot(Executor* executor, uint32_t slot_id)
{
    return &executor->slots[slot_id - 1];
}

Task* executor_wait(Executor* executor)
{
    if (executor_is_empty(executor))
    {
        return NULL;
    }
    for (;;)
    {
        Task* task = executor_update(executor);
        if (task)
        {
            return task;
        }
    }
}

static void executor_execute_slot(Executor* executor, uint32_t slot_id)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (slot->task->b_thread)
    {
        executor_execute_slot_thread(slot);
    }
    else
    {
        executor_execute_slot_process(slot);
    }
}

static void executor_flush_task(Executor* executor, Task* task)
{
    uint32_t slot_id = executor_find_empty_slot(executor);
    assert(slot_id);
    executor_set_slot_task(executor, slot_id, task);
    executor_execute_slot(executor, slot_id);
    executor->num_running_tasks += 1;
}

void executor_flush(Executor* executor)
{
    while (executor->num_running_tasks != executor->num_slots && array_size(executor->pending_tasks))
    {
        Task* task = executor->pending_tasks[0];
        array_remove_unordered(executor->pending_tasks, 0);
        executor_flush_task(executor, task);
    }
}

void executor_add_task(Executor* executor, Task* task)
{
    if (executor_is_full(executor))
    {
        array_push(executor->allocator, executor->pending_tasks, task);
        return;
    }
    executor_flush_task(executor, task);
}

Task* executor_create_thread_task(Executor* executor, int (*fn)(Task*, void*), void* ctx)
{
    Task* task = allocator_calloc(executor->allocator, 1, sizeof(Task));
    task->thread_fn = fn;
    task->ctx = ctx;
    task->b_thread = true;
    return task;
}

Task* executor_create_process_task(Executor* executor, char const* cmdline)
{
    Task* task = allocator_calloc(executor->allocator, 1, sizeof(Task));
    task->cmdline = string_from_c_str(executor->allocator, cmdline);
    task->b_thread = false;
    task->exit_code = EXIT_FAILURE;
    return task;
}

void executor_destroy_task(Executor* executor, Task* task)
{
    allocator_free(executor->allocator, task);
}

void executor_set_task_write_stdout_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes))
{
    task->write_stdout = fn;
}

void executor_set_task_write_stderr_fn(Task* task, void (*fn)(void* ctx, char const* buffer, size_t num_bytes))
{
    task->write_stderr = fn;
}

void executor_set_task_context(Task* task, void* ctx)
{
    task->ctx = ctx;
}

void* executor_get_task_context(Task* task)
{
    return task->ctx;
}

int executor_get_task_exit_code(Task* task)
{
    return task->exit_code;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes)
{
}

void executor_set_slot_task(Executor* executor, uint32_t slot_id, Task* task)
{
    executor_platform_set_slot(executor, slot_id, task);
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    slot->task = task;
    slot->b_finished = false;
    if (!task->b_thread)
    {
        if (task->write_stdout)
        {
            slot->read_stdout_ctx.write_buffer = task->write_stdout;
        }
        else
        {
            slot->read_stdout_ctx.write_buffer = executor_default_write_buffer;
        }
        if (task->write_stderr)
        {
            slot->read_stderr_ctx.write_buffer = task->write_stderr;
        }
        else
        {
            slot->read_stderr_ctx.write_buffer = slot->read_stdout_ctx.write_buffer;
        }
        slot->read_stdout_ctx.write_buffer_ctx = task->ctx;
        slot->read_stderr_ctx.write_buffer_ctx = task->ctx;
    }
}

#include <stdarg.h>

typedef enum ArgType
{
    ARG_NONE,
    ARG_STRING,
    ARG_INT,
    ARG_NODE,
} ArgType;

typedef enum PlaceholderType
{
    PLACEHOLDER_NAMED,
    PLACEHOLDER_POSITIONAL,
    PLACEHOLDER_AUTO_INDEXED
} PlaceholderType;

typedef struct Placeholder
{
    size_t index;
    ArgType arg_type;
    PlaceholderType type;
    char const* var_value;
} Placeholder;

static ArgType fmt_get_arg_type(char c)
{
    switch (c)
    {
    case 's': return ARG_STRING;
    case 'd': return ARG_INT;
    case 'n': return ARG_NODE;
    default: return ARG_NONE;
    }
}

static char const* fmt_parse_placeholder(char const* p, Placeholder* out_placeholder)
{
    char const* begin = p;
    while (*p && *p != '}' && *p != ':') p++;
    size_t len = p - begin;
    if (*p == ':')
    {
        p++;
        ArgType arg_type = fmt_get_arg_type(*p);
        if (arg_type == ARG_NONE)
        {
            return NULL;
        }
        out_placeholder->arg_type = arg_type;
        while (*p && *p != '}') p++;
    }
    else
    {
        out_placeholder->arg_type = ARG_STRING;
    }
    if (*p != '}') return NULL;
    p++;
    if (len > 0)
    {
        char* name = string_from_print(allocator_temp(), "%.*s", (int)len, begin);
        char* end;
        size_t idx = strtol(name, &end, 10);
        if (end == name + len)
        {
            out_placeholder->index = idx;
            out_placeholder->type = PLACEHOLDER_POSITIONAL;
        }
        else
        {
            out_placeholder->var_value = get_var(name);
            if (out_placeholder->var_value == NULL)
            {
                warn("unknown variable: %s", name);
                return NULL;
            }
            out_placeholder->type = PLACEHOLDER_NAMED;
        }
    }
    else
    {
        out_placeholder->type = PLACEHOLDER_AUTO_INDEXED;
    }
    return p;
}

static void** fmt_ensure_arg_loaded(size_t index, ArgType type, void** args, va_list* va)
{
    while (index >= array_size(args))
    {
        if (type == ARG_INT)
        {
            int v = va_arg(*va, int);
            array_push(allocator_temp(), args, (void*)(uintptr_t)v);
        }
        else
        {
            void* v = va_arg(*va, void*);
            if (!v) return NULL;
            array_push(allocator_temp(), args, v);
        }
    }
    return args;
}

static char const* fmt_get_placeholder_value(Placeholder const* placeholder, void** args)
{
    char const* result = NULL;
    switch (placeholder->type)
    {
    case PLACEHOLDER_NAMED:
        result = placeholder->var_value;
        break;
    case PLACEHOLDER_AUTO_INDEXED:
    case PLACEHOLDER_POSITIONAL:
        if (placeholder->arg_type == ARG_INT)
        {
            int i = (int)(uintptr_t)args[placeholder->index];
            result = string_from_print(allocator_temp(), result, "%d", i);
        }
        else if (placeholder->arg_type == ARG_NODE)
        {
            Node* node = args[placeholder->index];
            result = node->name;
        }
        else
        {
            result = args[placeholder->index];
        }
        break;
    }
    return result;
}

char* fmt_alloc_v(Allocator* allocator, char const* fmt_str, va_list* args)
{
    char const* p = fmt_str;
    char* result = NULL;
    size_t positional_index = 0;
    void** fmt_args = NULL;

    while (*p)
    {
        if (*p != '{')
        {
            string_putc(allocator, result, *p);
            p++;
            continue;
        }
        p++;
        Placeholder placeholder;
        p = fmt_parse_placeholder(p, &placeholder);
        if (p == NULL)
        {
            goto error;
        }
        if (placeholder.type == PLACEHOLDER_AUTO_INDEXED)
        {
            placeholder.index = positional_index++;
        }
        if (placeholder.type == PLACEHOLDER_AUTO_INDEXED || placeholder.type == PLACEHOLDER_POSITIONAL)
        {
            fmt_args = fmt_ensure_arg_loaded(placeholder.index, placeholder.arg_type, fmt_args, args);
            if (fmt_args == NULL)
            {
                goto error;
            }
        }
        char const* value = fmt_get_placeholder_value(&placeholder, fmt_args);
        string_concat_c_str(allocator, result, value);
    }
    return result;
error:
    fprintf(stderr, "fmt error: '%s'\n", fmt_str);
    return NULL;
}

char const* fmt(char const* fmt_str, ...)
{
    va_list v;
    va_start(v, fmt_str);
    char const* result = fmt_alloc_v(allocator_temp(), fmt_str, &v);
    va_end(v);
    return result;
}

char* fmt_alloc(Allocator* allocator, char const* fmt_str, ...)
{
    va_list v;
    va_start(v, fmt_str);
    char* result = fmt_alloc_v(allocator, fmt_str, &v);
    va_end(v);
    return result;
}

extern Node** nodes;

extern Allocator* node_allocator;

Graph* graph_create(Allocator* allocator, Node** targets, size_t num_targets)
{
    Graph* graph = allocator_calloc(allocator, 1, sizeof(Graph));
    graph->allocator = allocator;
    graph->sources = NULL;
    graph->hash_node_to_next_set = allocator_calloc(allocator, 1, sizeof(Hash));
    graph->hash_node_to_prev_set = allocator_calloc(allocator, 1, sizeof(Hash));
    graph->hash_node_to_b_finished = allocator_calloc(allocator, 1, sizeof(Hash));
    graph->hash_node_to_next_set->allocator = allocator;
    graph->hash_node_to_prev_set->allocator = allocator;
    graph->hash_node_to_b_finished->allocator = allocator;

    for (size_t i = 0; i != num_targets; i++)
    {
        graph_add_target(graph, targets[i]);
    }
    return graph;
}

static void graph_free_set(Hash* h)
{
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Set* set = (Set*)(uintptr_t)hash_value(h, i);
        hash_free(set);
    }
}

void graph_destroy(Graph* graph)
{
    array_free(graph->allocator, graph->sources);
    array_free(graph->allocator, graph->stack);
    graph_free_set(graph->hash_node_to_next_set);
    graph_free_set(graph->hash_node_to_prev_set);
    hash_free(graph->hash_node_to_next_set);
    hash_free(graph->hash_node_to_prev_set);
    hash_free(graph->hash_node_to_b_finished);
    allocator_free(graph->allocator, graph);
}

static Set* graph_get_node_next_set(Graph* graph, Node* node)
{
    uint32_t i = hash_insert(graph->hash_node_to_next_set, (uintptr_t)node);
    Set* next_set = (Set*)(uintptr_t)hash_value(graph->hash_node_to_next_set, i);
    if (next_set == NULL)
    {
        next_set = allocator_calloc(graph->allocator, 1, sizeof(Set));
        next_set->allocator = graph->allocator;
        hash_value(graph->hash_node_to_next_set, i) = (uintptr_t)next_set;
    }
    return next_set;
}

static Set* graph_get_node_prev_set(Graph* graph, Node* node)
{
    uint32_t i = hash_insert(graph->hash_node_to_prev_set, (uintptr_t)node);
    Set* prev_set = (Set*)(uintptr_t)hash_value(graph->hash_node_to_prev_set, i);
    if (prev_set == NULL)
    {
        prev_set = allocator_calloc(graph->allocator, 1, sizeof(Set));
        prev_set->allocator = graph->allocator;
        hash_value(graph->hash_node_to_prev_set, i) = (uintptr_t)prev_set;
    }
    return prev_set;
}

void graph_add_target(Graph* graph, Node* node)
{
    array_resize(graph->allocator, graph->stack, 0);
    array_push(graph->allocator, graph->stack, node);
    while (array_size(graph->stack))
    {
        Node* next = graph->stack[0];
        array_remove_unordered(graph->stack, 0);
        bool b_existed;
        hash_insert_check(graph->hash_node_to_b_finished, (uintptr_t)next, &b_existed);
        if (b_existed)
        {
            continue;
        }
        Set* prev_set = graph_get_node_prev_set(graph, next);
        size_t num_prev = array_size(next->dependencies);
        for (size_t i = 0; i != num_prev; i++)
        {
            Node* prev = next->dependencies[i];
            uint32_t j = hash_index(graph->hash_node_to_b_finished, (uintptr_t)prev);
            if (j == HASH_INVALID_INDEX || hash_value(graph->hash_node_to_b_finished, j) == false)
            {
                array_push(graph->allocator, graph->stack, prev);
                hash_insert(prev_set, (uintptr_t)prev);
                Set* next_set = graph_get_node_next_set(graph, prev);
                hash_insert(next_set, (uintptr_t)next);
            }
        }
        if (hash_size(prev_set) == 0 && !next->b_dynamic_indegree)
        {
            array_push(graph->allocator, graph->sources, next);
        }
    }
}

void graph_add_dynamic_edge(Graph* graph, Node* tail, Node* head)
{
    Set* head_next_set = graph_get_node_next_set(graph, head);
    hash_insert(head_next_set, (uintptr_t)tail);
    Set* tail_prev_set = graph_get_node_prev_set(graph, tail);
    hash_insert(tail_prev_set, (uintptr_t)head);
    graph_add_target(graph, head);
}

void graph_set_node_processed(Graph* graph, Node* node)
{
    hash_put(graph->hash_node_to_b_finished, (uintptr_t)node, (uint64_t)true);
    Set* next_set = (Set*)(uintptr_t)hash_get(graph->hash_node_to_next_set, (uintptr_t)node);
    if (!next_set)
    {
        return;
    }
    for (uint32_t i = next_set->begin; i != next_set->end; i = hash_next(next_set, i))
    {
        Node* next = (Node*)(uintptr_t)hash_key(next_set, i);
        Set* prev_set = (Set*)(uintptr_t)hash_get(graph->hash_node_to_prev_set, (uintptr_t)next);
        uint32_t j = hash_index(prev_set, (uintptr_t)node);
        if (j != HASH_INVALID_INDEX)
        {
            hash_remove(prev_set, j);
        }
        if (hash_size(prev_set) == 0)
        {
            array_push(graph->allocator, graph->sources, next);
        }
    }
    hash_reset(next_set);
}

Node* graph_pop(Graph* graph)
{
    if (array_size(graph->sources) == 0)
    {
        return NULL;
    }
    Node* node = graph->sources[0];
    array_remove_unordered(graph->sources, 0);
    return node;
}

Node** graph_get_unreachable_nodes(Graph* graph, Allocator* allocator)
{
    Node** unreachable_nodes = NULL;
    Hash* h = graph->hash_node_to_b_finished;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        bool b_finished = hash_value(h, i);
        Node* node = (Node*)(uintptr_t)hash_key(h, i);
        if (!b_finished)
        {
            array_push(allocator, unreachable_nodes, node);
        }
    }
    return unreachable_nodes;
}

bool b_node_default_excluded = false;

Allocator* node_allocator;
Node** nodes;
StringHash* hash_name_to_node;

extern char const* desc_color_exe;
extern char const* desc_color_input;
extern char const* desc_color_output;
extern char const* desc_color_reset;
extern char const* desc_color_error;
extern char const* desc_color_flag;
extern char const* desc_color_bright_flag;
extern bool b_content_hash;

SourceType get_source_type(char const* path)
{
    char const* ext_old = path_extension(path);
    char* ext = string_from_c_str(allocator_temp(), ext_old);
    string_tolower(ext);
    if (string_equal(ext, ".c"))
    {
        return SOURCE_TYPE_C;
    }
    if (string_equal(ext, ".cpp") || string_equal(ext, ".cc"))
    {
        return SOURCE_TYPE_CPP;
    }
    if (string_equal(ext, ".cppm") || string_equal(ext, ".ixx") ||
        string_equal(ext, ".ccm") || string_equal(ext, ".cxxm"))
    {
        return SOURCE_TYPE_CPPM;
    }
    if (string_equal(ext, ".s") || string_equal(ext, ".asm"))
    {
        return SOURCE_TYPE_ASM;
    }
    return SOURCE_TYPE_UNKNOWN;
}

void init_node(void)
{
    node_allocator = allocator_create_tiny(4096, 4096 * 4096);
    hash_name_to_node = allocator_calloc(node_allocator, 1, sizeof(StringHash));
    hash_name_to_node->allocator = node_allocator;
}

uint32_t node_make_file_type(FileType file_type, uint32_t ext_type)
{
    Node node = {.node_type = NODE_TYPE_FILE};
    node.file_type = file_type;
    node.file_ext_type = ext_type;
    return node.type;
}

uint32_t node_make_cmd_type(CmdType cmd_type, uint32_t ext_type)
{
    Node node = {.node_type = NODE_TYPE_CMD};
    node.cmd_type = cmd_type;
    node.cmd_ext_type = ext_type;
    return node.type;
}

uint32_t node_make_virtual_type(uint32_t ext_type)
{
    Node node = {.node_type = NODE_TYPE_VIRTUAL, .virtual_ext_type = ext_type};
    return node.type;
}

void node_prepare(Node* node)
{
}

void node_visit(Node* node, Graph* graph, Executor* executor)
{
    if (!node->b_dirty)
    {
        node->b_dirty = node->check_dirty(node);
    }
}

void node_virtual_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    node->processed(node, graph);
}

bool node_virtual_check_dirty(Node* node)
{
    return true;
}

void node_processed(Node* node, Graph* graph)
{
    graph_set_node_processed(graph, node);
}

Node* node_create(uint32_t type, char const* name, size_t num_bytes)
{
    Node* node = allocator_calloc(node_allocator, 1, num_bytes);
    node->type = type;
    node->b_default_excluded = b_node_default_excluded;
    if (name)
    {
        node_set_name(node, name);
    }
    node->prepare = node_prepare;
    switch (node->node_type)
    {
    case NODE_TYPE_VIRTUAL:
        node->visit = node_virtual_visit;
        node->check_dirty = node_virtual_check_dirty;
        node->processed = node_processed;
        break;
    case NODE_TYPE_CMD:
        node->visit = cmd_visit;
        node->check_dirty = cmd_check_dirty;
        node->before_execute = cmd_before_execute;
        node->after_execute = cmd_after_execute;
        node->processed = cmd_processed;
        node->prepare = cmd_prepare;
        if (node->cmd_type == CMD_TYPE_EXECUTABLE)
        {
            node->write_stdout_line_fn = cmd_write_stdout_line;
            node->write_stderr_line_fn = cmd_write_stderr_line;
        }
        break;
    case NODE_TYPE_FILE:
        node->visit = file_visit;
        node->check_dirty = file_check_dirty;
        node->mtime = os_get_mtime(name);
        node->processed = file_processed;
        break;
    }
    array_push(node_allocator, nodes, node);
    return node;
}

void node_set_name(Node* node, char const* name)
{
    bool b_existed;
    uint32_t i = hash_insert_check(hash_name_to_node, name, &b_existed);
    if (b_existed)
    {
        warn("The node named \"%s\" already exists.", name);
    }
    if (array_size(node->name))
    {
        uint32_t old_i = hash_index(hash_name_to_node, node->name);
        if (old_i != HASH_INVALID_INDEX)
        {
            hash_remove(hash_name_to_node, old_i);
        }
        string_clear(node->name);
    }
    node->name = string_append_c_str(node_allocator, node->name, name);
    hash_key(hash_name_to_node, i) = node->name;
    hash_value(hash_name_to_node, i) = (uintptr_t)node;
}

void node_set_alias(Node* node, char const* alias)
{
    bool b_existed;
    uint32_t i = hash_insert_check(hash_name_to_node, alias, &b_existed);
    if (b_existed)
    {
        warn("The node named \"%s\" already exists.", alias);
    }
    alias = string_from_c_str(node_allocator, alias);
    hash_key(hash_name_to_node, i) = alias;
    hash_value(hash_name_to_node, i) = (uintptr_t)node;
}

void node_add_debugger_argument(Node* node, char const* arg)
{
    char* new_arg = string_from_c_str(node_allocator, arg);
    array_push(node_allocator, node->debugger_run_arguments, new_arg);
}

void node_add_dependency(Node* node, Node* dependency)
{
    array_push(node_allocator, node->dependencies, dependency);
}

void node_ensure_prepared(Node* node)
{
    if (node->b_prepared) return;
    node->b_prepared = true;
    node->prepare(node);
}

void node_remove_dependency(Node* node, Node* dependency)
{
    array_compact(node->dependencies, array_pointer_compare, &dependency);
}

void node_set_check_dirty_fn(Node* node, FnCheckDirty* fn)
{
    node->check_dirty = fn;
}

void node_set_processed_fn(Node* node, FnProcessed* fn)
{
    node->processed = fn;
}

void node_set_extra_data(Node* node, void* extra_data)
{
    node->extra_data = extra_data;
}

void node_remove_edge(Node* tail, Node* head)
{
    array_compact(head->dependencies, array_pointer_compare, &tail);
}

static void file_check_space_and_backslash(Node* file)
{
    file->b_path_checked = true;
    for (size_t i = 0; i != array_size(file->path); i++)
    {
        int ch = file->path[i];
        if (ch == ' ')
        {
            file->b_has_space = true;
            if (file->b_has_backslash)
            {
                return;
            }
        }
        if (ch == '\\')
        {
            file->b_has_backslash = true;
            if (file->b_has_space)
            {
                return;
            }
        }
    }
}

bool file_path_has_space(Node* node)
{
    if (!node->b_path_checked)
    {
        file_check_space_and_backslash(node);
    }
    return node->b_has_space;
}

bool file_path_has_backslash(Node* node)
{
    if (!node->b_path_checked)
    {
        file_check_space_and_backslash(node);
    }
    return node->b_has_backslash;
}

uint64_t file_get_content_hash(Node* node)
{
    if (node->content_hash == 0 || node->b_dirty)
    {
        node->content_hash = utilities_compute_file_hash(node->path);
    }
    return node->content_hash;
}

char const* file_get_option_path(Node* node)
{
    char const* path;
    if (file_path_has_space(node))
    {
        path = fmt("\"{:n}\"", node);
    }
    else
    {
        path = node->path;
    }
    return path;
}

bool file_check_dirty(Node* node)
{
    if (node->mtime == 0)
    {
        return true;
    }
    if (node->build_cmd && node->build_cmd->b_dirty)
    {
        return true;
    }
    return false;
}

void file_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    node->processed(node, graph);
}

void file_processed(Node* node, Graph* graph)
{
    node_processed(node, graph);
}

Node* file_create(char const* path, size_t num_bytes)
{
    uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
    return node_create(node_type, path, num_bytes);
}

bool cmd_check_cache_dirty(Node* cmd, Cache* cache, CacheRecordCmd* r, bool* b_restat)
{
    if (r == NULL)
    {
        return true;
    }
    if (array_size(r->inputs) != array_size(cmd->inputs) || array_size(r->outputs) != array_size(cmd->outputs))
    {
        return true;
    }
    for (size_t i = 0; i != array_size(cmd->inputs); i++)
    {
        CacheFile* cf = &r->inputs[i];
        Node* input = cmd->inputs[i];
        char const* cache_file_path = cache_get_string(cache, cf->id);
        char const* input_path = input->path;
        if (!string_equal(input_path, cache_file_path))
        {
            return true;
        }
        if (input->mtime != cf->build_time)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(input);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = input->mtime;
            }
        }
    }
    for (size_t i = 0; i != array_size(cmd->outputs); i++)
    {
        Node* output = cmd->outputs[i];
        CacheFile* cf = &r->outputs[i];
        char const* output_path = output->path;
        char const* cache_file_path = cache_get_string(cache, cf->id);
        if (!string_equal(output_path, cache_file_path))
        {
            return true;
        }
        if (output->mtime != cf->build_time)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(output);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = output->mtime;
            }
        }
    }
    if (cmd->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        if (array_size(cmd->cmdline) != array_size(r->cmdline))
        {
            return true;
        }
        if (!string_equal(cmd->cmdline, r->cmdline))
        {
            return true;
        }
    }
    for (size_t i = 0; i != array_size(r->implicit_inputs); i++)
    {
        CacheFile* cf = &r->implicit_inputs[i];
        char const* path = cache_get_string(cache, cf->id);
        Node* input = find_node(path);
        if (!input)
        {
            uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
            input = node_create(node_type, path, sizeof(Node));
            input->b_default_excluded = true;
        }
        if (input->mtime == 0 || cf->build_time != input->mtime)
        {
            if (!b_content_hash)
            {
                return true;
            }
            uint64_t current_hash = file_get_content_hash(input);
            if (current_hash == 0 || current_hash != cf->content_hash)
            {
                return true;
            }
            else
            {
                *b_restat = true;
                cf->build_time = input->mtime;
            }
        }
    }
    return false;
}

bool cmd_check_dirty(Node* node)
{
    for (size_t i = 0; i != array_size(node->inputs); i++)
    {
        Node* input = node->inputs[i];
        if (input->b_dirty)
        {
            return true;
        }
    }
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        if (node->outputs[i]->mtime == 0)
        {
            return true;
        }
    }
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordCmd* record = cache_find_cmd_record(cache, node->name);
        bool b_restat = false;
        if (cmd_check_cache_dirty(node, cache, record, &b_restat))
        {
            return true;
        }
        else if (b_restat)
        {
            cache_write_cmd_record(cache, record);
        }
    }
    else
    {
        return true;
    }
    return false;
}

static int thread_task_fn_wrapper(Task* task, void* ctx)
{
    Node* cmd = ctx;
    return cmd->fn(cmd);
}

static void cmd_write_output(
    Node* cmd,
    char** output_line,
    void (*write_line_fn)(Node* cmd, char const* line),
    char const* buffer,
    size_t num_bytes)
{
    for (char const* p = buffer; p != buffer + num_bytes; p++)
    {
        if (*p == '\n')
        {
            if (array_size(*output_line) && array_last(*output_line) == '\r')
            {
                array_pop(*output_line);
            }
            array_push(node_allocator, *output_line, '\0');
            array_pop(*output_line);
            write_line_fn(cmd, *output_line);
            array_resize(node_allocator, *output_line, 0);
        }
        else
        {
            array_push(node_allocator, *output_line, *p);
        }
    }
    if (num_bytes == 0 && array_size(*output_line))
    {
        array_push(node_allocator, *output_line, '\0');
        array_pop(*output_line);
        write_line_fn(cmd, *output_line);
    }
}

static void cmd_write_stderr_fn_wrapper(void* ctx, char const* buffer, size_t num_bytes)
{
    Node* cmd = ctx;
    cmd_write_output(cmd, &cmd->stderr_line, cmd->write_stderr_line_fn, buffer, num_bytes);
}

static void cmd_write_stdout_fn_wrapper(void* ctx, char const* buffer, size_t num_bytes)
{
    Node* cmd = ctx;
    cmd_write_output(cmd, &cmd->stdout_line, cmd->write_stdout_line_fn, buffer, num_bytes);
}

void cmd_visit(Node* node, Graph* graph, Executor* executor)
{
    node_visit(node, graph, executor);
    if (node->b_dirty)
    {
        if (executor)
        {
            node->before_execute(node);
            Task* task = cmd_create_task(node, executor);
            executor_add_task(executor, task);
        }
    }
    else
    {
        node->processed(node, graph);
    }
}

void cmd_prepare(Node* node)
{
}

void cmd_processed(Node* node, Graph* graph)
{
    node_processed(node, graph);
}

void cmd_add_input(Node* node, Node* file)
{
    for (size_t i = 0; i != array_size(node->inputs); i++)
    {
        Node* input = node->inputs[i];
        if (input == file)
        {
            return;
        }
    }
    array_push(node_allocator, node->inputs, file);
    node_add_dependency(node, file);
}

void cmd_add_output(Node* node, Node* file)
{
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        if (output == file)
        {
            return;
        }
    }
    array_push(node_allocator, node->outputs, file);
    file->build_cmd = node;
    node_add_dependency(file, node);
    if (node->name == NULL)
    {
        string_printf(node_allocator, node->name, "cmd: make %s", file->path);
    }
}

void cmd_update_output_mtime(Node* node)
{
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        uint64_t new_mtime = os_get_mtime(output->path);
        if (new_mtime != output->mtime)
        {
            output->mtime = new_mtime;
            output->b_dirty = true;
        }
    }
}

void cmd_after_execute(Node* node)
{
    cmd_update_output_mtime(node);
    Cache* cache = get_cache();
    if (cache)
    {
        Allocator* temp_allocator = allocator_temp();
        CacheRecordCmd cmd_record = {0};
        cmd_record.name = node->name;
        if (node->cmd_type == CMD_TYPE_EXECUTABLE)
        {
            cmd_record.cmdline = node->cmdline;
        }
        size_t num_inputs = array_size(node->inputs);
        array_resize(temp_allocator, cmd_record.inputs, num_inputs);
        for (size_t i = 0; i != num_inputs; i++)
        {
            Node* file_node = node->inputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_in_file_record(cache, path);
            cmd_record.inputs[i].id = record->id;
            cmd_record.inputs[i].build_time = file_node->mtime;
            cmd_record.inputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        size_t num_outputs = array_size(node->outputs);
        array_resize(temp_allocator, cmd_record.outputs, num_outputs);
        for (size_t i = 0; i != num_outputs; i++)
        {
            Node* file_node = node->outputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_out_file_record(cache, path);
            cmd_record.outputs[i].id = record->id;
            cmd_record.outputs[i].build_time = file_node->mtime;
            cmd_record.outputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        size_t num_implicit_inputs = array_size(node->implicit_inputs);
        array_resize(temp_allocator, cmd_record.implicit_inputs, num_implicit_inputs);
        for (size_t i = 0; i != num_implicit_inputs; i++)
        {
            Node* file_node = node->implicit_inputs[i];
            char const* path = file_node->path;
            CacheRecordFile* record = cache_get_or_add_in_file_record(cache, path);
            cmd_record.implicit_inputs[i].id = record->id;
            cmd_record.implicit_inputs[i].build_time = file_node->mtime;
            cmd_record.implicit_inputs[i].content_hash = b_content_hash ? file_get_content_hash(file_node) : 0;
        }
        cache_write_cmd_record(cache, &cmd_record);
    }
}

void cmd_before_execute(Node* node)
{
    char const* desc = cmd_get_description(node);
    if (array_size(desc))
    {
        puts(desc);
    }
    array_resize(node_allocator, node->implicit_inputs, 0);
    for (size_t i = 0; i != array_size(node->outputs); i++)
    {
        Node* output = node->outputs[i];
        os_ensure_dir_existed(output->path);
    }
}

void cmd_add_input_file_option(Node* node, char const* option, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option(node, option, path, OPTION_INPUT);
    cmd_add_input(node, file);
}

void cmd_add_output_file_option(Node* node, char const* option, Node* file)
{
    char const* path;
    if (file_path_has_space(file))
    {
        Allocator* allocator = allocator_temp();
        path = string_from_print(allocator, "\"%s\"", file->path);
    }
    else
    {
        path = file->path;
    }
    cmd_add_option(node, option, path, OPTION_OUTPUT);
    cmd_add_output(node, file);
}

static char const* get_option_color(OptionType type)
{
    if (type == OPTION_EXE) return desc_color_exe;
    if (type == OPTION_INPUT) return desc_color_input;
    if (type == OPTION_OUTPUT) return desc_color_output;
    if (type == OPTION_FLAG) return desc_color_flag;
    if (type == OPTION_BRIGHT_FLAG) return desc_color_bright_flag;
    if (type == OPTION_HIDDEN) return NULL;
    assert(false);
    return NULL;
}

void cmd_add_option(Node* node, char const* option, char const* param, OptionType type)
{
    if (type == OPTION_HIDDEN)
    {
        if (option)
        {
            if (array_size(node->extra_options))
            {
                string_putc(node_allocator, node->extra_options, ' ');
            }
            string_printf(node_allocator, node->extra_options, "%s", option);
        }
        if (param)
        {
            if (!option && array_size(node->extra_options))
            {
                string_putc(node_allocator, node->extra_options, ' ');
            }
            string_printf(node_allocator, node->extra_options, "%s", param);
        }
        return;
    }
    if (type != OPTION_EXE && type != OPTION_NONE)
    {
        string_putc(node_allocator, node->cmdline, ' ');
        string_putc(node_allocator, node->description, ' ');
    }
    if (option)
    {
        string_concat_c_str(node_allocator, node->cmdline, option);
        string_printf(node_allocator, node->description, "%s%s%s", get_option_color(OPTION_FLAG), option, desc_color_reset);
    }
    if (param)
    {
        string_concat_c_str(node_allocator, node->cmdline, param);
        char const* color = get_option_color(type);
        if (color)
        {
            string_printf(node_allocator, node->description, "%s%s%s", color, param, desc_color_reset);
        }
        else
        {
            string_concat_c_str(node_allocator, node->description, param);
        }
    }
}

void cmd_remove_input(Node* node, Node* file)
{
    node_remove_dependency(node, file);
    array_compact(node->inputs, array_pointer_compare, &file);
}

void cmd_remove_output(Node* node, Node* file)
{
    node_remove_dependency(file, node);
    array_compact(node->outputs, array_pointer_compare, &file);
}

char const* cmd_get_description(Node* node)
{
    char const* desc = node->description;
    if (!desc && node->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        return node->cmdline;
    }
    return desc;
}

char const* cmd_get_cmdline(Node* node)
{
    if (node->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        return node->cmdline;
    }
    return NULL;
}

Task* cmd_create_task(Node* cmd, Executor* executor)
{
    Task* task;
    if (cmd->cmd_type == CMD_TYPE_EXECUTABLE)
    {
        Allocator* temp_allocator = allocator_temp();
        char* cmdline = cmd->cmdline;
        if (cmd->extra_options)
        {
            cmdline = string_from_print(temp_allocator, "%s %s", cmdline, cmd->extra_options);
        }
        task = executor_create_process_task(executor, cmdline);
        executor_set_task_write_stdout_fn(task, cmd_write_stdout_fn_wrapper);
        executor_set_task_write_stderr_fn(task, cmd_write_stderr_fn_wrapper);
        if (cmd->env_node)
        {
            executor_set_task_env_block(task, cmd->env_node->env_block);
        }
    }
    else
    {
        task = executor_create_thread_task(executor, thread_task_fn_wrapper, cmd);
    }
    executor_set_task_context(task, cmd);
    return task;
}

void cmd_set_source_location(Node* node, char const* file, int line)
{
    node->file = file;
    node->line = line;
}

void cmd_set_before_execute_fn(Node* node, FnBeforeExecute* fn)
{
    node->before_execute = fn;
}

void cmd_set_after_execute_fn(Node* node, FnAfterExecute* fn)
{
    node->after_execute = fn;
}

void cmd_set_env(Node* node, Node* env)
{
    assert(!env || env->node_type == NODE_TYPE_FILE);
    assert(node->node_type == NODE_TYPE_CMD && node->cmd_type == CMD_TYPE_EXECUTABLE);
    if (node->env_node)
    {
        cmd_remove_input(node, node->env_node);
    }
    if (env)
    {
        cmd_add_input(node, env);
        node->env_node = env;
    }
    else
    {
        node->env_node = NULL;
        return;
    }
    if (env->file_type != FILE_TYPE_ENV)
    {
        env->file_type = FILE_TYPE_ENV;
    }
}

void cmd_write_stdout_line(Node* cmd, char const* line)
{
    size_t num_bytes = array_size(line);
    array_push_v(node_allocator, cmd->std_output, line, num_bytes);
    string_putc(node_allocator, cmd->std_output, '\n');
}

void cmd_write_stderr_line(Node* cmd, char const* line)
{
    size_t num_bytes = array_size(line);
    array_push_v(node_allocator, cmd->std_error, line, num_bytes);
    string_putc(node_allocator, cmd->std_error, '\n');
}

void cmd_set_write_output_line_fn(Node* node, void (*fn)(Node* cmd, char const* line))
{
    node->write_stdout_line_fn = fn;
}

void cmd_set_write_stderr_line_fn(Node* node, void (*fn)(Node* cmd, char const* line))
{
    node->write_stderr_line_fn = fn;
}

void cmd_add_implicit_input(Node* node, char const* dep)
{
    Node* file = get_or_add_file(dep);
    array_push(node_allocator, node->implicit_inputs, file);
}

void cmd_set_description(Node* node, char const* string)
{
    array_resize(node_allocator, node->description, 0);
    string_concat_c_str(node_allocator, node->description, string);
}

Node** get_all_nodes(void)
{
    return nodes;
}

Node* get_or_add_file(char const* path)
{
    Node* node = find_node(path);
    if (!node)
    {
        uint32_t node_type = node_make_file_type(FILE_TYPE_NORMAL, 0);
        node = node_create(node_type, path, sizeof(Node));
    }
    return node;
}

Node* get_or_add_file_with_type(char const* path, FileType type)
{
    Node* file = get_or_add_file(path);
    file->file_type = type;
    return file;
}

Node* get_or_add_node(uint32_t type, char const* name, size_t num_bytes)
{
    Node* node = find_node(name);
    if (!node)
    {
        node = node_create(type, name, num_bytes);
    }
    return node;
}

Node* add_thread_cmd(FnThread* fn, void* ctx, char const* file, int line)
{
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_THREAD, 0);
    Node* node = node_create(node_type, NULL, sizeof(Node));
    node->fn = fn;
    node->ctx = ctx;
    cmd_set_source_location(node, file, line);
    return node;
}

Node* add_process_cmd(char const* string, char const* file, int line)
{
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, 0);
    Node* n = node_create(node_type, NULL, sizeof(Node));
    if (string)
    {
        cmd_add_option(n, NULL, string, OPTION_EXE);
    }
    cmd_set_source_location(n, file, line);
    return n;
}

Node* add_process_cmd_from_exe_node(Node* exe, char const* name, char const* file, int line)
{
    Allocator* allocator = allocator_temp();
    char* cmdline;
    if (file_path_has_space(exe))
    {
        cmdline = string_from_print(allocator, "\"%s\"", exe->path);
    }
    else
    {
        cmdline = string_new(allocator, array_size(exe->path), exe->path);
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        path_slash_to_backslash(cmdline);
    }
    Node* cmd = add_process_cmd(cmdline, file, line);
    cmd_add_input(cmd, exe);
    node_set_name(cmd, name);
    return cmd;
}

Node* find_node(char const* name)
{
    assert(hash_name_to_node);
    return (Node*)(uintptr_t)hash_get(hash_name_to_node, name);
}

bool has_dependency(Node* node, Node* dependency)
{
    return array_find(node->dependencies, array_pointer_compare, &dependency);
}

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

static inline bool is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline int is_dec_digit(int c)
{
    return c >= '0' && c <= '9';
}

static inline bool is_ident_char(char c)
{
    return is_ident_start(c) || is_dec_digit(c);
}

static void test_finder_skip_bom(FILE* file)
{
    uint8_t ch[3] = {0};
    size_t n = fread(ch, 1, 3, file);
    if (n != 3 || ch[0] != 0xEF || ch[1] != 0xBB || ch[2] != 0xBF)
    {
        fseek(file, 0, SEEK_SET);
    }
}

static char const* test_finder_skip_space(char const* p)
{
    while (*p)
    {
        if (!isspace(*p))
        {
            break;
        }
        ++p;
    }
    return p;
}

static void test_finder_get_line(Allocator* allocator, FILE* file, char** line)
{
    while (true)
    {
        int ch = fgetc(file);
        if (ch == '\n')
        {
            array_push(allocator, *line, ch);
            break;
        }
        if (ch == EOF)
        {
            if (array_size(*line) == 0)
            {
                break;
            }
            array_push(allocator, *line, '\n');
            break;
        }
        array_push(allocator, *line, ch);
    }
    if (array_size(*line) == 0)
    {
        return;
    }
    array_push(allocator, *line, '\0');
    array_pop(*line);
}

static char* test_finder_match_entry(char const* line, Allocator* allocator)
{
    char const* p = test_finder_skip_space(line);
    char* entry = NULL;
    if (memcmp(p, "TEST(", 5) == 0)
    {
        p += 5;
        p = test_finder_skip_space(p);
        int c = *p;
        if (!is_ident_start(c))
        {
            goto NOT_FOUND;
        }
        while (c = *p++, is_ident_char(c)) array_push(allocator, entry, c);
    }
    if (entry)
    {
        array_push(allocator, entry, '\0');
        array_pop(entry);
        return entry;
    }
NOT_FOUND:
    if (entry)
    {
        array_free(allocator, entry);
    }
    return NULL;
}

char** test_finder_get_entries(Allocator* allocator, char const* src_path)
{
    FILE* file = os_fopen(src_path, "r");
    if (file == NULL)
    {
        return NULL;
    }
    test_finder_skip_bom(file);
    char** entries = NULL;
    Allocator* temp_allocator = allocator_temp();
    char* line = NULL;
    while (true)
    {
        array_resize(temp_allocator, line, 0);
        test_finder_get_line(temp_allocator, file, &line);
        if (array_size(line) == 0)
        {
            break;
        }
        char* entry = test_finder_match_entry(line, allocator);
        if (entry)
        {
            array_push(allocator, entries, entry);
        }
    }
    fclose(file);
    return entries;
}

char const* desc_color_exe = ANSI_COLOR_GREEN;
char const* desc_color_input = ANSI_COLOR_BLUE;
char const* desc_color_output = ANSI_COLOR_MAGENTA;
char const* desc_color_reset = ANSI_COLOR_RESET;
char const* desc_color_error = ANSI_COLOR_RED;
char const* desc_color_flag = ANSI_COLOR_GRAY;
char const* desc_color_bright_flag = ANSI_BRIGHT_YELLOW;

StringPtrHash* variables = NULL;

void var_on_cwd_changed(void)
{
    if (variables == NULL)
    {
        init_var();
    }
    else
    {
        Allocator* temp_allocator = allocator_temp();
        char const* cwd = os_get_cwd(temp_allocator);
        char const* self_exe_fullpath = os_get_current_exe_path(temp_allocator);
        if (path_is_under_directory(self_exe_fullpath, cwd))
        {
            self_exe_fullpath = path_lexically_relative(self_exe_fullpath, cwd, temp_allocator);
        }
        char const* self_name = path_stem(self_exe_fullpath, temp_allocator);
        set_var("self", self_exe_fullpath);
        set_var("self_name", self_name);
        set_var("workspace", cwd);
    }
}

void init_var(void)
{
    if (!os_is_terminal_supports_color())
    {
        desc_color_exe = "";
        desc_color_input = "";
        desc_color_output = "";
        desc_color_reset = "";
        desc_color_error = "";
        desc_color_flag = "";
        desc_color_bright_flag = "";
    }

    variables = allocator_calloc(allocator_c(), 1, sizeof(StringPtrHash));
    variables->allocator = allocator_c();
    struct
    {
        char const* key;
        char const* value;
    } map[] = {
        {"mode", "header_only"},
        {"out_dir", "build"},
        {"obj_dir", "obj"},
        {"exe_ext", EXE_EXT},
        {"dll_ext", DLL_EXT},
        {"lib_ext", LIB_EXT},
        {"obj_ext", OBJ_EXT},
        {"implib_ext", ".lib"},
        {"platform", get_platform_string(CURRENT_PLATFORM)},
        {"color_exe", desc_color_exe},
        {"color_in", desc_color_input},
        {"color_out", desc_color_output},
        {"color_flag", desc_color_flag},
        {"color_bright_flag", desc_color_bright_flag},
        {"color_reset", desc_color_reset},
        {"#", desc_color_reset},
    };
    for (size_t i = 0; i != static_array_size(map); i++)
    {
        set_var(map[i].key, map[i].value);
    }
    var_on_cwd_changed();
}

void set_var(char const* name, char const* value)
{
    uint32_t i = hash_index(variables, name);
    if (i != HASH_INVALID_INDEX)
    {
        if (value == NULL)
        {
            char const* key = hash_value(variables, i);
            char const* value = hash_key(variables, i);
            array_free(allocator_c(), key);
            array_free(allocator_c(), value);
            hash_remove(variables, i);
            return;
        }
        char* v = hash_value(variables, i);
        array_resize(allocator_c(), v, 0);
        string_concat_c_str(allocator_c(), v, value);
        hash_value(variables, i) = v;
        return;
    }
    char* k = string_from_c_str(allocator_c(), name);
    char* v = string_from_c_str(allocator_c(), value);
    hash_put(variables, k, v);
}

char const* get_var(char const* name)
{
    uint32_t i = hash_index(variables, name);
    if (i != HASH_INVALID_INDEX)
    {
        return hash_value(variables, i);
    }
    return NULL;
}

void destroy_var(void)
{
    if (!variables)
    {
        return;
    }
    StringPtrHash* h = variables;
    for (size_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* key = hash_key(h, i);
        array_free(allocator_c(), key);
        char const* value = hash_value(h, i);
        array_free(allocator_c(), value);
    }
    hash_free(h);
}



#include <assert.h>

extern Allocator* node_allocator;
extern ArchitectureType default_architecture_type;
extern ToolchainType default_toolchain;

static char const* get_ar_name()
{
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC) return "lib";
    if (default_toolchain == TOOLCHAIN_TYPE_LLVM)
    {
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
        return "llvm-lib";
#elif CURRENT_PLATFORM == PLATFORM_MACOS
        return "ar rcs";
#else
        return "llvm-ar rcs";
#endif
    }
    if (default_toolchain == TOOLCHAIN_TYPE_GCC) return "ar rcs";
    if (default_toolchain == TOOLCHAIN_TYPE_ZIG) return "zig ar rcs";
    assert(false && "unknown toolchain");
    return NULL;
}

static char const* get_ar_option_out()
{
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC ||
        (default_toolchain == TOOLCHAIN_TYPE_LLVM && CURRENT_PLATFORM == PLATFORM_WINDOWS))
    {
        return "/out:";
    }
    else return "";
}

static void ar_cmd_prepare(Node* node)
{
    ArCmd* cmd = (ArCmd*)node;
    Node* env = get_toolchain_env_node(cmd->toolchain, default_architecture_type);
    cmd_set_env(node, env);
    char const* ar_name = get_ar_name();
    char const* opt_out = get_ar_option_out();
    cmd_add_option(node, NULL, ar_name, OPTION_EXE);
    cmd_add_option(node, opt_out, cmd->output->path, OPTION_OUTPUT);
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
    }
    for (size_t i = 0; i != array_size(cmd->ar_inputs); i++)
    {
        Node* input = cmd->ar_inputs[i];
        cmd_add_option(node, NULL, input->path, OPTION_INPUT);
        cmd_add_input(node, input);
    }
    cmd_prepare(node);
}

Node* ar_cmd_create(Node* output, char const* file, int line)
{
    assert(output->build_cmd == NULL);
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_AR);
    Node* cmd = node_create(node_type, NULL, sizeof(ArCmd));
    ArCmd* ar_cmd = (ArCmd*)cmd;
    ar_cmd->toolchain = default_toolchain;
    ar_cmd->output = output;
    ar_cmd->set_inputs = allocator_calloc(node_allocator, 1, sizeof(Set));
    ar_cmd->set_inputs->allocator = node_allocator;
    ar_cmd->prepare = ar_cmd_prepare;
    cmd_set_source_location(cmd, file, line);
    cmd_add_output(cmd, output);
    return cmd;
}

void ar_cmd_add_input(Node* node, Node* input)
{
    ArCmd* cmd = (ArCmd*)node;
    array_push(node_allocator, cmd->ar_inputs, input);
}

void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain)
{
    ArCmd* cmd = (ArCmd*)node;
    cmd->toolchain = toolchain;
}




#include <assert.h>

extern Allocator* node_allocator;
extern ToolchainType default_toolchain;
extern CLanguageStandard default_c_std;
extern CppLanguageStandard default_cpp_std;
extern OptimizationType default_optimization_type;
extern ArchitectureType default_architecture_type;
extern bool b_generate_debug_info;
extern bool b_cache_header_dependencies;

void c_compile_cmd_prepare_gcc(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_msvc(Node* node, CCompileCmd* cmd);
void c_compile_cmd_prepare_zigcc(Node* node, CCompileCmd* cmd);

void compile_cmdline_node_make_cmdline_gcc(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_msvc(CompileCmdline* compile_cmdline);
void compile_cmdline_node_make_cmdline_zigcc(CompileCmdline* compile_cmdline);

extern SourceType get_source_type(char const* path);

char const* get_toolchain_bmi_extension(ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_MSVC)
    {
        return ".ifc";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM || toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        return ".pcm";
    }
    if (toolchain_type == TOOLCHAIN_TYPE_GCC)
    {
        return ".gcm";
    }
    assert(false);
    return NULL;
}

void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type)
{
    for (uint32_t i = set->begin; i != set->end; i = hash_next(set, i))
    {
        char const* param = hash_key(set, i);
        cmd_add_option(node, option, param, option_type);
    }
}

void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type)
{
    for (size_t i = 0; i != array_size(set); i++)
    {
        char const* param = set[i];
        cmd_add_option(node, option, param, option_type);
    }
}

void compile_cmdline_node_make_cmdline(CompileCmdline* node)
{
    ToolchainType toolchain = node->cmd->toolchain;
    switch (toolchain)
    {
    case TOOLCHAIN_TYPE_LLVM: compile_cmdline_node_make_cmdline_llvm(node); break;
    case TOOLCHAIN_TYPE_GCC: compile_cmdline_node_make_cmdline_gcc(node); break;
    case TOOLCHAIN_TYPE_MSVC: compile_cmdline_node_make_cmdline_msvc(node); break;
    case TOOLCHAIN_TYPE_ZIG: compile_cmdline_node_make_cmdline_zigcc(node); break;
    default: assert(false);
    }
}

static void compile_cmdline_node_visit(Node* node, Graph* graph, Executor* executor)
{
    if (node->b_dirty)
    {
        CompileCmdline* compile_cmdline = (CompileCmdline*)node;
        compile_cmdline_node_make_cmdline(compile_cmdline);
        node->b_dirty = false;
    }
    node->processed(node, graph);
}

static Node* compile_cmdline_node_create(CCompileCmd* c_compile_cmd)
{
    char const* name = fmt("make_cmdline: {}", c_compile_cmd->name);
    uint32_t type = node_make_virtual_type(VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE);
    Node* node = node_create(type, name, sizeof(CompileCmdline));
    CompileCmdline* cmdline_node = (CompileCmdline*)node;
    node->visit = compile_cmdline_node_visit;
    node->b_dirty = true;
    cmdline_node->cmd = c_compile_cmd;
    return node;
}

void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map)
{
    for (size_t i = 0; i != array_size(cmd->import_names); i++)
    {
        char const* name = cmd->import_names[i];
        Node* bmi = cmd->import_bmis[i];
        if (string_equal(name, "std"))
        {
            if (bmi == NULL)
                bmi = cmd->import_bmis[i] = get_or_create_std_module_for_compile_cmd(cmd);
            cmd->b_import_std = true;
        }
        if (string_equal(name, "std.compat"))
        {
            if (bmi == NULL)
                bmi = cmd->import_bmis[i] = get_or_create_std_compat_module_for_compile_cmd(cmd);
            cmd->b_import_std = true;
        }
        if (bmi == NULL)
        {
            continue;
        }
        bool b_existed;
        uint32_t index = hash_insert_check(out_map, name, &b_existed);
        if (!b_existed)
        {
            hash_value(out_map, index) = bmi;
            if (bmi->build_cmd)
            {
                c_compile_cmd_get_all_imports((CCompileCmd*)bmi->build_cmd, out_map);
            }
        }
    }
}

static void c_compile_cmd_add_module_inputs(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (size_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Node* bmi = hash_value(h, i);
        cmd_add_input(node, bmi);
        if (bmi->build_cmd)
        {
            CCompileCmd* bmi_build_cmd = (CCompileCmd*)bmi->build_cmd;
            obj_add_link_node(cmd->out_obj, bmi_build_cmd->out_obj);
        }
    }
}

static void c_compile_cmd_prepare(Node* node)
{
    CCompileCmd* cmd = (CCompileCmd*)node;

    node_add_dependency(node, node->make_cmdline);
    cmd_add_input(node, cmd->src);

    if (cmd->b_cpp)
    {
        if (cmd->export_bmi || cmd->export_name)
        {
            cmd->source_type = SOURCE_TYPE_CPPM;
        }
        if (cmd->source_type == SOURCE_TYPE_CPPM)
        {
            if (cmd->export_bmi == NULL)
            {
                cmd->export_bmi = module_from_src(cmd->src);
                cmd_add_output(node, cmd->export_bmi);
            }
        }
        c_compile_cmd_add_module_inputs(node, cmd);
    }
    switch (cmd->toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC: c_compile_cmd_prepare_msvc(node, cmd); break;
    case TOOLCHAIN_TYPE_LLVM: c_compile_cmd_prepare_llvm(node, cmd); break;
    case TOOLCHAIN_TYPE_GCC: c_compile_cmd_prepare_gcc(node, cmd); break;
    case TOOLCHAIN_TYPE_ZIG: c_compile_cmd_prepare_zigcc(node, cmd); break;
    default: assert(false);
    }

    cmd_prepare(node);
}

bool c_compile_cmd_check_dirty(Node* node)
{
    if (cmd_check_dirty(node))
    {
        return true;
    }
    CCompileCmd* cmd = (CCompileCmd*)node;
    if (cmd->scan_deps_cmd && cmd->scan_deps_cmd->b_dirty)
    {
        return true;
    }
    return false;
}

Node* c_compile_cmd_create(Node* input, Node* out_obj, char const* file, int line)
{
    assert(input);
    assert(out_obj);
    assert(out_obj->build_cmd == NULL);

    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE);
    char const* name = fmt("compile: {:n}", out_obj);
    Node* node = node_create(type, name, sizeof(CCompileCmd));
    CCompileCmd* cmd = (CCompileCmd*)node;
    {
        SourceType source_type = get_source_type(input->path);
        cmd->toolchain = default_toolchain;
        cmd->includes = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        cmd->includes->allocator = node_allocator;
        cmd->defines = allocator_calloc(node_allocator, 1, sizeof(StringSet));
        cmd->defines->allocator = node_allocator;
        cmd->src = input;
        cmd->out_obj = out_obj;
        cmd->source_type = source_type;
        cmd->b_cpp = source_type == SOURCE_TYPE_CPP || source_type == SOURCE_TYPE_CPPM;
        cmd->make_cmdline = compile_cmdline_node_create(cmd);
        cmd->prepare = c_compile_cmd_prepare;
        cmd->check_dirty = c_compile_cmd_check_dirty;
        cmd->b_color_diagnostics = os_is_terminal_supports_color();
        cmd->b_generate_debug_info = b_generate_debug_info;
        cmd->b_cache_header_dependencies = b_cache_header_dependencies;
        cmd->c_std = default_c_std;
        cmd->cpp_std = default_cpp_std;
        cmd->optimization_type = default_optimization_type;
        cmd->arch = default_architecture_type;
    }
    out_obj->build_cmd = node;
    node->file = file;
    node->line = line;
    return node;
}

void c_compile_cmd_add_include_directory(Node* node, char const* dir)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    bool b_existed;
    uint32_t i = hash_insert_check(cmd->includes, dir, &b_existed);
    if (!b_existed)
    {
        hash_key(cmd->includes, i) = string_from_c_str(node_allocator, dir);
    }
}

void c_compile_cmd_add_define(Node* node, char const* define)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    bool b_existed;
    uint32_t i = hash_insert_check(cmd->defines, define, &b_existed);
    if (!b_existed)
    {
        hash_key(cmd->defines, i) = string_from_c_str(node_allocator, define);
    }
}

void c_compile_cmd_add_flag(Node* node, char const* flag)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    char* f = string_from_c_str(node_allocator, flag);
    array_push(node_allocator, cmd->flags, f);
}

void c_compile_cmd_set_c_std(Node* cmd, CLanguageStandard c_std)
{
    CCompileCmd* cc = (CCompileCmd*)cmd;
    cc->c_std = c_std;
}

void c_compile_cmd_set_cpp_std(Node* cmd, CppLanguageStandard cpp_std)
{
    CCompileCmd* cc = (CCompileCmd*)cmd;
    cc->cpp_std = cpp_std;
}

void c_compile_cmd_set_arch(Node* node, ArchitectureType arch)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->arch = arch;
}

void c_compile_cmd_set_optimization_type(Node* node, OptimizationType type)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->optimization_type = type;
}

void c_compile_cmd_add_import(Node* node, char const* name, Node* bmi)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    if (!string_equal(name, "std") && !string_equal(name, "std.compat") && bmi == NULL)
    {
        warn("c_compile_cmd_add_import: named module: %s must not empty", name);
        return;
    }
    char* import_name = string_from_c_str(node_allocator, name);
    array_push(node_allocator, cmd->import_names, import_name);
    array_push(node_allocator, cmd->import_bmis, bmi);
}

void c_compile_cmd_set_export(Node* node, char const* name, Node* bmi)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->export_bmi = bmi;
    cmd->export_bmi->build_cmd = node;
    c_compile_cmd_set_export_name(node, name);
    if (cmd->export_map)
    {
        hash_put(cmd->export_map, cmd->export_name, bmi);
    }
}

void c_compile_cmd_set_export_name(Node* node, char const* name)
{
    CCompileCmd* cmd = (CCompileCmd*)node;
    array_resize(node_allocator, cmd->export_name, 0);
    if (name)
    {
        string_concat_c_str(node_allocator, cmd->export_name, name);
    }
}

void c_compile_cmd_set_export_map(CCompileCmd* cmd, StringPtrHash* map)
{
    cmd->export_map = map;
}

void c_compile_cmd_set_import_map(CCompileCmd* cmd, StringPtrHash* map)
{
    cmd->import_map = map;
}

void c_compile_cmd_add_self_build_options(Node* node)
{
    extern ToolchainType self_build_toolchain;
    extern ArchitectureType get_self_build_arch(void);

    CCompileCmd* cmd = (CCompileCmd*)node;
    cmd->toolchain = self_build_toolchain;
    cmd->internal_flag = true;
    c_compile_cmd_set_arch(node, get_self_build_arch());
    if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        c_compile_cmd_set_c_std(node, C_LANGUAGE_STANDARD_23);
    }
    else
    {
        if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM || cmd->toolchain == TOOLCHAIN_TYPE_ZIG)
        {
            c_compile_cmd_add_flag(node, "-Wno-microsoft-anon-tag");
        }
        c_compile_cmd_add_flag(node, "-fms-extensions");
        c_compile_cmd_add_flag(node, "-Wno-deprecated-declarations");
    }
}

#include <assert.h>

extern Allocator* node_allocator;

typedef struct ModuleMapper ModuleMapper;

struct ModuleMapper
{
    CCompileCmd* cmd;
    char const* content;
    Node* file;
};

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd);
void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

static void compile_cmdline_node_make_cmdline_gcc_c_cpp_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (array_size(cmd->export_name) || array_size(cmd->import_names))
    {
        cmd_add_option(node, "-fmodules", NULL, OPTION_FLAG);
        cmd_add_input_file_option(node, "-fmodule-mapper=", cmd->module_mapper);
    }
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, "-fdiagnostics-color", NULL, OPTION_HIDDEN);
    }
}

static void compile_cmdline_node_make_cmdline_gcc_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "g++", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_gcc_c_cpp_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_gcc_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "gcc", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_gcc_c_cpp_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_gcc_cpp_module(CompileCmdline* compile_cmdline)
{
    compile_cmdline_node_make_cmdline_gcc_cpp(compile_cmdline);
}

static void compile_cmdline_node_make_cmdline_gcc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, NULL, "gcc", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->arch)
    {
        cmd_add_option(node, cmd->arch == ARCH_X64 ? "-m64" : cmd->arch == ARCH_X86 ? "-m32"
                                                                                    : NULL,
                       NULL, OPTION_FLAG);
    }
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, "-g", NULL, OPTION_FLAG);
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    if (!b_pure_asm && cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void compile_cmdline_node_make_cmdline_gcc(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM || cmd->export_name)
    {
        compile_cmdline_node_make_cmdline_gcc_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_gcc_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_gcc_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_gcc_asm(compile_cmdline);
    }
    else
    {
        assert(false);
    }
}

void compile_cmd_after_execute_llvm_gcc(Node* node);
void c_compile_cmd_llvm_gcc_setup_after_execute_fn(Node* node, CCompileCmd* cmd, void (*fn)(Node*));
char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator);

static char* module_mapper_to_string(ModuleMapper* data, Allocator* allocator)
{
    char* result = NULL;
    CCompileCmd* cmd = data->cmd;
    for (size_t i = 0; i != array_size(cmd->import_names); i++)
    {
        char const* name = cmd->import_names[i];
        Node* bmi = cmd->import_bmis[i];
        if (bmi == NULL)
        {
            continue;
        }
        string_printf(allocator, result, "%s %s\n", name, bmi->path);
    }
    if (array_size(cmd->export_name))
    {
        string_printf(allocator, result, "%s %s\n", cmd->export_name, cmd->export_bmi->path);
    }
    return result;
}

static bool module_mapper_gen_cmd_check_dirty(Node* node)
{
    if (cmd_check_dirty(node))
    {
        return true;
    }
    ModuleMapper* mapper = node->extra_data;
    Node* output = mapper->file;
    mapper->content = module_mapper_to_string(mapper, allocator_c());
    char const* old_content = os_read_all(allocator_temp(), output->path);
    if (!string_equal(old_content, mapper->content))
    {
        return true;
    }
    return false;
}

static int module_mapper_gen_cmd_thread_fn(Node* node)
{
    ModuleMapper* mapper = node->extra_data;
    Node* output = mapper->file;
    char const* content = mapper->content;
    if (content == NULL)
    {
        content = module_mapper_to_string(mapper, allocator_c());
    }
    else
    {
        mapper->content = NULL;
    }
    os_write_all(output->path, content, array_size(content));
    if (content)
    {
        array_free(allocator_c(), content);
    }
    return EXIT_SUCCESS;
}

static Node* module_mapper_gen_cmd_create(Node* output, CCompileCmd* cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_THREAD, 0);
    char const* name = fmt("gen: {:n}", output);
    Node* node = node_create(type, name, sizeof(Node));
    ModuleMapper* mapper = allocator_calloc(node_allocator, 1, sizeof(ModuleMapper));
    mapper->cmd = cmd;
    mapper->file = output;
    node->extra_data = mapper;
    node->check_dirty = module_mapper_gen_cmd_check_dirty;
    node->fn = module_mapper_gen_cmd_thread_fn;
    cmd_add_output(node, output);
    return node;
}

void c_compile_cmd_prepare_gcc(Node* node, CCompileCmd* cmd)
{
    if (cmd->export_name && cmd->export_bmi == NULL)
    {
        cmd->export_bmi = module_from_src(cmd->src);
    }
    if (cmd->export_bmi)
    {
        cmd_add_output(node, cmd->export_bmi);
    }
    if (cmd->export_bmi || array_size(cmd->import_names))
    {
        if (cmd->module_mapper == NULL)
        {
            char const* mapper_path = determine_imtermediate_path(cmd->out_obj->path, "mappers", ".txt", allocator_temp());
            cmd->module_mapper = get_or_add_file(mapper_path);
            module_mapper_gen_cmd_create(cmd->module_mapper, cmd);
            cmd_add_input(node, cmd->module_mapper);
        }
    }
    cmd_add_output(node, cmd->out_obj);
    if (cmd->scan_deps_cmd == NULL && cmd->b_cache_header_dependencies)
    {
        c_compile_cmd_llvm_gcc_setup_after_execute_fn(node, cmd, compile_cmd_after_execute_llvm_gcc);
    }
}


#include <assert.h>
#include <stdio.h>

extern Allocator* node_allocator;
extern ToolchainType default_toolchain;

void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

static char const* get_arch_option_clang_or_gcc(ArchitectureType arch)
{
    if (arch == ARCH_UNSPECIFIED) return NULL;
    if (arch == ARCH_X64) return "-m64";
    if (arch == ARCH_X86) return "-m32";
    return NULL;
}

static char const* get_optimization_option_clang_or_gcc(OptimizationType optimization)
{
    switch (optimization)
    {
    case OPTIMIZATION_TYPE_UNSPECIFIED: return NULL;
    case OPTIMIZATION_TYPE_DEBUG: return "-O0";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "-O3";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "-Os";
    default: assert(false); return NULL;
    }
}

static char const* get_cpp_std_option_clang_or_gcc(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case CPP_LANGUAGE_STANDARD_98: return "-std=c++98";
    case CPP_LANGUAGE_STANDARD_11: return "-std=c++11";
    case CPP_LANGUAGE_STANDARD_14: return "-std=c++14";
    case CPP_LANGUAGE_STANDARD_17: return "-std=c++17";
    case CPP_LANGUAGE_STANDARD_20: return "-std=c++20";
    case CPP_LANGUAGE_STANDARD_23: return "-std=c++23";
    case CPP_LANGUAGE_STANDARD_26: return "-std=c++26";
    default: assert(false); return NULL;
    }
}

static char const* get_c_std_option_clang_or_gcc(CLanguageStandard c_std)
{
    switch (c_std)
    {
    case C_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case C_LANGUAGE_STANDARD_99: return "-std=c99";
    case C_LANGUAGE_STANDARD_11: return "-std=c11";
    case C_LANGUAGE_STANDARD_17: return "-std=c17";
    case C_LANGUAGE_STANDARD_23: return "-std=c2x";
    default: assert(false); return NULL;
    }
}

bool is_clang_supported_module_extension(char const* path)
{
    Allocator* allocator = allocator_temp();
    char* ext = (char*)path_extension(path);
    ext = string_from_c_str(allocator, ext);
    string_tolower(ext);
    if (string_equal(ext, ".cppm") || string_equal(ext, ".cxxm") || string_equal(ext, "ccm"))
    {
        return true;
    }
    return false;
}

char const* c_compile_cmd_get_depfile_path(CCompileCmd* cmd)
{
    char const* depfile_path;
    if (file_path_has_space(cmd->out_obj))
    {
        depfile_path = fmt("\"{:n}.d\"", cmd->out_obj);
    }
    else
    {
        depfile_path = fmt("{:n}.d", cmd->out_obj);
    }
    return depfile_path;
}

void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd)
{
    char const* depfile_path = c_compile_cmd_get_depfile_path(cmd);
    cmd_add_option(node, "-MMD -MF ", depfile_path, OPTION_HIDDEN);
}

void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void cmd_after_execute_parse_depfile(Node* node)
{
    char const* path = node->ctx;
    FILE* f = os_fopen(path, "rb");
    if (!f)
    {
        return;
    }
    DepfileParser parser;
    depfile_parser_init(&parser, f);
    char* dep = NULL;
    DepfileItemType item_type;
    Allocator* allocator = allocator_create_tiny(4096, 4096);
    while (depfile_parser_next(&parser, allocator, &dep, &item_type))
    {
        switch (item_type)
        {
        case DEPFILE_ITEM_NORMAL_DEP:
            if (os_file_exists(dep))
            {
                cmd_add_implicit_input(node, dep);
            }
            break;
        default:;
        }
        array_resize(allocator, dep, 0);
    }
    allocator_destroy(allocator);
    fclose(f);
    os_remove_file(path);
}

void compile_cmd_after_execute_llvm_gcc(Node* node)
{
    cmd_after_execute_parse_depfile(node);
    cmd_after_execute(node);
}

static void compile_cmd_before_execute_rename_occupied_cc_file(Node* cmd)
{
    void c_toolchain_rename_to_old(char const* path);

    cmd_before_execute(cmd);
    CCompileCmd* cc = (CCompileCmd*)cmd;
    if (cc->export_bmi)
    {
        if (!os_file_writable(cc->export_bmi->path))
        {
            c_toolchain_rename_to_old(cc->export_bmi->path);
        }
    }
}

void c_compile_cmd_get_all_imports(CCompileCmd* cmd, StringPtrHash* out_map);
static Node* bmi_to_obj_cmd_create(CCompileCmd* cc, char const* file, int line)
{
    extern bool b_generate_debug_info;

    Node* obj = cc->out_obj;
    Node* pcm = cc->export_bmi;
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_BMI_TO_OBJ);
    Node* cmd = node_create(node_type, fmt("gen: {:n}", obj), sizeof(BmiToObjCmd));
    cmd_set_source_location(cmd, file, line);
    BmiToObjCmd* cmd_bmi_to_obj = (BmiToObjCmd*)cmd;
    cmd_bmi_to_obj->c_compile_cmd = cc;
    char const* compiler = default_toolchain == TOOLCHAIN_TYPE_ZIG ? "zig c++" : "clang++";
    cmd_add_option(cmd, NULL, compiler, OPTION_EXE);
    cmd_add_output_file_option(cmd, "-o ", obj);
    if (b_generate_debug_info)
    {
        cmd_add_option(cmd, "-g", NULL, OPTION_FLAG);
    }
    cmd_add_option(cmd, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(cmd, NULL, pcm);
    StringPtrHash map = {.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cc, &map);
    for (uint32_t i = map.begin; i != map.end; i = hash_next(&map, i))
    {
        char const* name = hash_key(&map, i);
        Node* bmi = hash_value(&map, i);
        if (!bmi)
        {
            continue;
        }
        char const* option = fmt("-fmodule-file={}=", name);
        cmd_add_input_file_option(cmd, option, bmi);
        cmd_add_input(cmd, bmi);
    }
    cmd->prepare(cmd);
    return cmd;
}
void c_compile_cmd_llvm_gcc_setup_after_execute_fn(Node* node, CCompileCmd* cmd, void (*fn)(Node*))
{
    node->after_execute = fn;
    node->ctx = string_from_print(node_allocator, "%s.d", cmd->out_obj->path);
}

void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd)
{
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        assert(cmd->export_bmi);
        cmd_add_output(node, cmd->export_bmi);
    }
    cmd_add_output(node, cmd->out_obj);
    if (cmd->scan_deps_cmd == NULL && cmd->b_cache_header_dependencies)
    {
        c_compile_cmd_llvm_gcc_setup_after_execute_fn(node, cmd, compile_cmd_after_execute_llvm_gcc);
    }
    if (cmd->export_bmi)
    {
        cmd->before_execute = compile_cmd_before_execute_rename_occupied_cc_file;
    }
}

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->arch)
    {
        cmd_add_option(node, get_arch_option_clang_or_gcc(cmd->arch), NULL, OPTION_FLAG);
    }
    if (cmd->optimization_type)
    {
        cmd_add_option(node, get_optimization_option_clang_or_gcc(cmd->optimization_type), NULL, OPTION_FLAG);
    }
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, "-g", NULL, OPTION_FLAG);
    }
    if (cmd->b_cpp)
    {
        char const* cpp_std_option = get_cpp_std_option_clang_or_gcc(cmd->cpp_std);
        if (cpp_std_option)
        {
            cmd_add_option(node, cpp_std_option, NULL, OPTION_FLAG);
        }
    }
    else
    {
        char const* c_std_option = get_c_std_option_clang_or_gcc(cmd->c_std);
        if (c_std_option)
        {
            cmd_add_option(node, c_std_option, NULL, OPTION_FLAG);
        }
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
}

void compile_cmdline_node_make_cmdline_llvm_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, "-fcolor-diagnostics", NULL, OPTION_HIDDEN);
        cmd_add_option(node, "-fansi-escape-codes", NULL, OPTION_HIDDEN);
    }
}

void compile_cmdline_node_make_cmdline_llvm_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "clang", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
}

void compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* module_name = hash_key(h, i);
        Node* bmi = hash_value(h, i);
        char const* option = fmt("-fmodule-file={}=", module_name);
        cmd_add_input_file_option(node, option, bmi);
    }
}

static void compile_cmdline_node_make_cmdline_llvm_cpp_module(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "clang++", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_output_file_option(node, "-fmodule-output=", cmd->export_bmi);
    if (!is_clang_supported_module_extension(cmd->src->path))
    {
        cmd_add_option(node, "-x c++-module", NULL, OPTION_FLAG);
    }
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_llvm_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "clang++", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_llvm_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, NULL, "clang", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, "-g", NULL, OPTION_FLAG);
    }
    if (cmd->arch)
    {
        cmd_add_option(node, get_arch_option_clang_or_gcc(cmd->arch), NULL, OPTION_FLAG);
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    if (!b_pure_asm && cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_llvm_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_llvm_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_llvm_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_llvm_asm(compile_cmdline);
    }
    else
    {
        assert(false);
    }
}

#include <assert.h>

extern Allocator* node_allocator;

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

static char const* get_optimization_option_cl(OptimizationType optimization)
{
    switch (optimization)
    {
    case OPTIMIZATION_TYPE_DEBUG: return "/Od";
    case OPTIMIZATION_TYPE_RELEASE_FAST: return "/Ox";
    case OPTIMIZATION_TYPE_RELEASE_SMALL: return "/Os";
    case OPTIMIZATION_TYPE_UNSPECIFIED: return NULL;
    default: assert(false); return NULL;
    }
}

static char const* get_cpp_std_option_cl(CppLanguageStandard cpp_std)
{
    switch (cpp_std)
    {
    case CPP_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case CPP_LANGUAGE_STANDARD_98: return "/std:c++98";
    case CPP_LANGUAGE_STANDARD_11: return "/std:c++11";
    case CPP_LANGUAGE_STANDARD_14: return "/std:c++14";
    case CPP_LANGUAGE_STANDARD_17: return "/std:c++17";
    case CPP_LANGUAGE_STANDARD_20: return "/std:c++20";
    case CPP_LANGUAGE_STANDARD_23: return "/std:c++latest";
    case CPP_LANGUAGE_STANDARD_26: return "/std:c++latest";
    default: assert(false); return NULL;
    }
}

static char const* get_c_std_option_cl(CLanguageStandard c_std)
{
    switch (c_std)
    {
    case C_LANGUAGE_STANDARD_UNSPECIFIED: return NULL;
    case C_LANGUAGE_STANDARD_99: return "/std:c11";
    case C_LANGUAGE_STANDARD_11: return "/std:c11";
    case C_LANGUAGE_STANDARD_17: return "/std:c17";
    default: return "/std:clatest";
    }
}

void c_compile_cmd_write_buffer_msvc(Node* node, char const* line)
{
    CCompileCmd* cmd = (CCompileCmd*)node->ctx;
    char const* cwd = cmd->cwd;
    if (cmd->b_cache_header_dependencies)
    {
        if (strncmp(line, cmd->msvc_show_include_prefix, cmd->show_include_prefix_len) == 0)
        {
            char* dep = (char*)line + cmd->show_include_prefix_len;
            while (isspace(*dep)) dep += 1;
            if (path_is_absolute(dep))
            {
                if (path_is_under_directory(dep, cwd))
                {
                    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
                    dep = path_lexically_relative(dep, cwd, temp_allocator);
                    path_backslash_to_slash(dep);
                }
                else
                {
                    dep = NULL;
                }
            }
            if (dep)
            {
                cmd_add_implicit_input(node, dep);
            }
            return;
        }
    }
    if (string_equal(line, cmd->input_filename))
    {
        return;
    }
    cmd_write_stderr_line(node, line);
}

void c_compile_cmd_init_msvc(CCompileCmd* cmd)
{
    if (cmd->input_filename == NULL)
    {
        extern char* msvc_show_include_prefix;
        cmd->input_filename = path_filename(cmd->src->path, node_allocator);
        cmd->cwd = os_get_cwd(node_allocator);
        cmd->msvc_show_include_prefix = msvc_show_include_prefix;
        cmd->show_include_prefix_len = strlen(cmd->msvc_show_include_prefix);
    }
}

void c_compile_cmd_prepare_msvc(Node* node, CCompileCmd* cmd)
{
    c_compile_cmd_init_msvc(cmd);
    Node* env = msvc_get_env_node(cmd->toolchain, cmd->arch);
    cmd_set_env(node, env);
    cmd_add_output(node, cmd->out_obj);
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        assert(cmd->export_bmi);
        cmd_add_output(node, cmd->export_bmi);
    }
    if (cmd->b_generate_debug_info && cmd->source_type != SOURCE_TYPE_ASM)
    {
        if (cmd->pdb == NULL)
        {
            cmd->pdb = get_or_add_file(fmt("{:n}.pdb", cmd->out_obj));
        }
        cmd_add_output(node, cmd->pdb);
    }
    cmd->ctx = cmd;
    cmd->write_stdout_line_fn = c_compile_cmd_write_buffer_msvc;
    cmd->write_stderr_line_fn = c_compile_cmd_write_buffer_msvc;
}

static bool is_cl_supported_module_extension(char const* path)
{
    Allocator* allocator = allocator_temp();
    char* ext = (char*)path_extension(path);
    ext = string_from_c_str(allocator, ext);
    string_tolower(ext);
    if (string_equal(ext, ".ixx"))
    {
        return true;
    }
    return false;
}

void compile_cmdline_node_make_cmdline_msvc_scan_deps_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
    if (cmd->b_cpp)
    {
        if (!is_cl_supported_module_extension(cmd->src->path))
        {
            cmd_add_option(node, "/TP", NULL, OPTION_FLAG);
        }
        if (cmd->export_name || cmd->export_bmi)
        {
            cmd_add_option(node, "/interface", NULL, OPTION_FLAG);
        }
    }
    if (array_size(cmd->import_names) || cmd->export_name)
    {
        cmd_add_option(node, "/EHsc", NULL, OPTION_FLAG);
    }
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->optimization_type)
    {
        cmd_add_option(node, get_optimization_option_cl(cmd->optimization_type), NULL, OPTION_FLAG);
    }
    if (cmd->b_cpp)
    {
        char const* cpp_std_option = get_cpp_std_option_cl(cmd->cpp_std);
        if (cpp_std_option)
        {
            cmd_add_option(node, cpp_std_option, NULL, OPTION_FLAG);
        }
    }
    else
    {
        char const* c_std_option = get_c_std_option_cl(cmd->c_std);
        if (c_std_option)
        {
            cmd_add_option(node, c_std_option, NULL, OPTION_FLAG);
        }
    }
    compile_cmdline_node_append_string_set_options(node, "/I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "/D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
}

void compile_cmdline_node_make_cmdline_msvc_common(Node* node, CCompileCmd* cmd)
{
    cmd_add_option(node, NULL, "cl", OPTION_EXE);
    cmd_add_output_file_option(node, "/Fo:", cmd->out_obj);
    cmd_add_option(node, "/c", NULL, OPTION_FLAG);
    compile_cmdline_node_make_cmdline_msvc_scan_deps_common(node, cmd);
    if (cmd->b_generate_debug_info)
    {
        assert(cmd->pdb);
        cmd_add_option(node, "/Zi", NULL, OPTION_FLAG);
        cmd_add_output_file_option(node, "/Fd", cmd->pdb);
    }
}

static void compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(Node* node, CCompileCmd* cmd)
{
    StringPtrHash* h = &(StringPtrHash){.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cmd, h);
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* module_name = hash_key(h, i);
        Node* bmi = hash_value(h, i);
        if (file_path_has_space(bmi))
        {
            char const* option = fmt("/reference \"{}=", module_name);
            cmd_add_option(node, option, bmi->path, OPTION_INPUT);
            cmd_add_option(node, "\"", NULL, OPTION_NONE);
        }
        else
        {
            char const* option = fmt("/reference {}=", module_name);
            cmd_add_input_file_option(node, option, bmi);
        }
    }
}

static void compile_cmdline_node_make_cmdline_msvc_cppm(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    cmd_add_output_file_option(node, "/ifcOutput ", cmd->export_bmi);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, "/showIncludes", NULL, OPTION_HIDDEN);
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_msvc_c_cpp(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_msvc_common(node, cmd);
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option(node, "/showIncludes", NULL, OPTION_HIDDEN);
    }
    compile_cmdline_node_make_cmdline_msvc_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_msvc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    Allocator* temp = allocator_temp();
    char const* ext_old = path_extension(cmd->src->path);
    char* ext = string_from_c_str(temp, ext_old);
    string_tolower(ext);
    if (string_equal(ext, ".asm"))
    {
        cmd_add_option(node, NULL, cmd->arch == ARCH_X64 ? "ml64" : "ml", OPTION_EXE);
        cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
        cmd_add_option(node, "/quiet", NULL, OPTION_FLAG);
        cmd_add_option(node, "/c", NULL, OPTION_FLAG);
        if (cmd->b_generate_debug_info)
        {
            cmd_add_option(node, "/Zi", NULL, OPTION_FLAG);
        }
        cmd_add_option(node, "/Fo", cmd->out_obj->path, OPTION_OUTPUT);
        cmd_add_output(node, cmd->out_obj);
        cmd_add_input_file_option(node, NULL, cmd->src);
        compile_cmdline_node_append_string_set_options(node, "/I", cmd->includes, OPTION_BRIGHT_FLAG);
        compile_cmdline_node_append_string_set_options(node, "/D", cmd->defines, OPTION_FLAG);
        compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    }
    else
    {
        assert(false && "MSVC does not support .s/.S files; use .asm with MASM syntax");
    }
}

void compile_cmdline_node_make_cmdline_msvc(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_msvc_cppm(node, cmd);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_msvc_asm(compile_cmdline);
    }
    else
    {
        compile_cmdline_node_make_cmdline_msvc_c_cpp(node, cmd);
    }
}

static void set_filenames(Node** nodes, size_t size, char const** filenames)
{
    for (size_t i = 0; i != size; i++)
    {
        char const* path = nodes[i]->path;
        size_t len = strlen(path);
        char const* p = path + len - 1;
        while (p != path && *p != '/' && *p != '\\') --p;
        if (p != path)
        {
            p += 1;
        }
        filenames[i] = p;
    }
}

#include <assert.h>

void c_compile_cmd_prepare_llvm(Node* node, CCompileCmd* cmd);
bool is_clang_supported_module_extension(char const* path);
void cmd_add_option_mmd_mf(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_append_string_set_options(Node* node, char const* option, StringSet* set, OptionType option_type);
void compile_cmdline_node_append_string_array_options(Node* node, char const* option, char** set, OptionType option_type);

void c_compile_cmd_prepare_zigcc(Node* node, CCompileCmd* cmd)
{
    extern char* g_zig_target;
    if (cmd->toolchain == TOOLCHAIN_TYPE_ZIG && g_zig_target)
    {
        c_compile_cmd_add_flag(node, fmt("-target {}", g_zig_target));
    }
    c_compile_cmd_prepare_llvm(node, cmd);
}

void compile_cmdline_node_make_cmdline_llvm(CompileCmdline* compile_cmdline);

static void compile_cmdline_node_make_cmdline_zigcc_common(Node* node, CCompileCmd* cmd)
{
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd);
    if (cmd->b_color_diagnostics)
    {
        cmd_add_option(node, "-fcolor-diagnostics", NULL, OPTION_HIDDEN);
        cmd_add_option(node, "-fansi-escape-codes", NULL, OPTION_HIDDEN);
        cmd_add_option(node, "-fdiagnostics-color=always", NULL, OPTION_HIDDEN);
    }
}

static void compile_cmdline_node_make_cmdline_zigcc_cpp_module(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig c++", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->export_bmi);
    cmd_add_option(node, "--precompile", NULL, OPTION_FLAG);
    if (!is_clang_supported_module_extension(cmd->src->path))
    {
        cmd_add_option(node, "-x c++-module", NULL, OPTION_FLAG);
    }
    if (cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

void compile_cmdline_node_make_cmdline_zigcc_c(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig cc", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
}

static void compile_cmdline_node_make_cmdline_zigcc_cpp(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    cmd_add_option(node, NULL, "zig c++", OPTION_EXE);
    compile_cmdline_node_make_cmdline_llvm_gcc_c_cpp_common(node, cmd);
    compile_cmdline_node_make_cmdline_zigcc_common(node, cmd);
    compile_cmdline_node_make_cmdline_llvm_add_module_ref_options(node, cmd);
}

static void compile_cmdline_node_make_cmdline_zigcc_asm(CompileCmdline* compile_cmdline)
{
    Node* node = (Node*)compile_cmdline->cmd;
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    char const* ext = path_extension(cmd->src->path);
    bool b_pure_asm = string_equal(ext, ".s");
    cmd_add_option(node, NULL, "zig cc", OPTION_EXE);
    cmd_add_output_file_option(node, "-o ", cmd->out_obj);
    cmd_add_option(node, "-c", NULL, OPTION_FLAG);
    cmd_add_input_file_option(node, NULL, cmd->src);
    if (cmd->arch)
    {
        char const* arch_opt = NULL;
        if (cmd->arch == ARCH_X64) arch_opt = "-m64";
        else if (cmd->arch == ARCH_X86) arch_opt = "-m32";
        if (arch_opt)
            cmd_add_option(node, arch_opt, NULL, OPTION_FLAG);
    }
    if (cmd->b_generate_debug_info)
    {
        cmd_add_option(node, "-g", NULL, OPTION_FLAG);
    }
    compile_cmdline_node_append_string_set_options(node, "-I", cmd->includes, OPTION_BRIGHT_FLAG);
    compile_cmdline_node_append_string_set_options(node, "-D", cmd->defines, OPTION_FLAG);
    compile_cmdline_node_append_string_array_options(node, NULL, cmd->flags, OPTION_FLAG);
    if (!b_pure_asm && cmd->b_cache_header_dependencies && cmd->scan_deps_cmd == NULL)
    {
        cmd_add_option_mmd_mf(node, cmd);
    }
}

void compile_cmdline_node_make_cmdline_zigcc(CompileCmdline* compile_cmdline)
{
    CCompileCmd* cmd = (CCompileCmd*)compile_cmdline->cmd;
    if (cmd->source_type == SOURCE_TYPE_CPPM)
    {
        compile_cmdline_node_make_cmdline_zigcc_cpp_module(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_C)
    {
        compile_cmdline_node_make_cmdline_zigcc_c(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_CPP)
    {
        compile_cmdline_node_make_cmdline_zigcc_cpp(compile_cmdline);
    }
    else if (cmd->source_type == SOURCE_TYPE_ASM)
    {
        compile_cmdline_node_make_cmdline_zigcc_asm(compile_cmdline);
    }
    else
    {
        assert(false);
    }
}

#include <assert.h>

extern Allocator* node_allocator;
extern ToolchainType default_toolchain;

char const* msvc_find_std_module_source(bool b_compat);
char const* get_toolchain_bmi_extension(ToolchainType toolchain_type);
char* determine_imtermediate_path(char const* src_path, char const* sub_dir, char const* ext, Allocator* allocator);

static char const* probe_module_source(Allocator* alloc, char** include_dirs, char const** suffixes, size_t suffix_count)
{
    for (size_t i = 0; i < array_size(include_dirs); i++)
    {
        for (size_t j = 0; j < suffix_count; j++)
        {
            char* candidate = string_from_print(alloc, "%s/%s", include_dirs[i], suffixes[j]);
            if (os_file_exists(candidate))
            {
                return candidate;
            }
        }
    }
    return NULL;
}

static char** get_compiler_include_dirs(Allocator* alloc, char const* compiler_exe)
{
    char* cmd = string_from_print(alloc, "echo | %s -E -Wp,-v -x c++ - 2>&1", compiler_exe);
    FILE* f = os_popen(cmd, "r");
    if (!f) return NULL;

    char** dirs = NULL;
    char buffer[4096];
    bool in_search_list = false;

    while (fgets(buffer, sizeof(buffer), f))
    {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[len - 1] = '\0';
            len--;
        }

        if (strstr(buffer, "#include <...> search starts here:"))
        {
            in_search_list = true;
            continue;
        }
        if (strstr(buffer, "End of search list."))
        {
            break;
        }

        if (in_search_list)
        {
            char* path = buffer;
            while (*path == ' ') path++;
            if (*path)
            {
                char* normal = path_lexically_normal(path, alloc);
                array_push(alloc, dirs, normal);
            }
        }
    }

    os_pclose(f);
    return dirs;
}

static char const* find_std_module_source(ToolchainType toolchain, bool b_compat)
{
    static char const* cached_std = NULL;
    static char const* cached_compat = NULL;

    char const** cache_ptr = b_compat ? &cached_compat : &cached_std;
    if (*cache_ptr) return *cache_ptr;

    Allocator* temp = allocator_temp();
    char const* result = NULL;

    if (toolchain == TOOLCHAIN_TYPE_MSVC || (CURRENT_PLATFORM == PLATFORM_WINDOWS && toolchain != TOOLCHAIN_TYPE_GCC))
    {
        result = msvc_find_std_module_source(b_compat);
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_LLVM || toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        char const* compiler = (toolchain == TOOLCHAIN_TYPE_GCC) ? "g++" : "clang++";
        char** include_dirs = get_compiler_include_dirs(temp, compiler);

        if (include_dirs)
        {
            char const* stdcpp_suffixes[] = {
                b_compat ? "bits/std.compat.cc" : "bits/std.cc",
            };
            result = probe_module_source(temp, include_dirs, stdcpp_suffixes, static_array_size(stdcpp_suffixes));

            if (!result)
            {
                char const* cpp_suffixes[] = {
                    b_compat ? "v1/std.compat.cppm" : "v1/std.cppm",
                    b_compat ? "../share/libc++/v1/std.compat.cppm" : "../share/libc++/v1/std.cppm",
                };
                ;
                result = probe_module_source(temp, include_dirs, cpp_suffixes, static_array_size(cpp_suffixes));
            }
        }
    }

    if (result)
    {
        *cache_ptr = string_from_c_str(allocator_c(), result);
    }
    return *cache_ptr;
}

static Node* get_std_module_source_node(ToolchainType toolchain_type)
{
    char const* path = find_std_module_source(toolchain_type, false);
    if (path)
    {
        return get_or_add_src(path);
    }
    else
    {
        return NULL;
    }
}

static Node* get_std_compat_module_source_node(ToolchainType toolchain_type)
{
    char const* path = find_std_module_source(toolchain_type, true);
    if (path)
    {
        return get_or_add_file(path);
    }
    else
    {
        return NULL;
    }
}

static char const* get_std_module_bmi_path(
    char const* source,
    ToolchainType toolchain_Type,
    ArchitectureType arch,
    OptimizationType optimization_type,
    CppLanguageStandard cpp_std)
{
    char const* filename = path_filename(source, allocator_temp());
    char const* toolchain_string = get_toolchain_string(toolchain_Type);
    char const* arch_string = get_arch_string(arch);
    char const* optimization_type_string = get_optimization_string(optimization_type);
    char const* std_string = get_cpp_std_string(cpp_std);
    char const* ext = get_toolchain_bmi_extension(toolchain_Type);
    char* path = (char*)fmt("{out_dir}/modules/std/{}", toolchain_string);
    if (arch_string)
    {
        string_printf(allocator_temp(), path, "/%s", arch_string);
    }
    if (optimization_type_string)
    {
        string_printf(allocator_temp(), path, "/%s", optimization_type_string);
    }
    if (std_string)
    {
        string_printf(allocator_temp(), path, "/%s", std_string);
    }

    string_printf(allocator_temp(), path, "/%s%s", filename, ext);
    return path;
}

static Node* obj_from_bmi(Node* bmi)
{
    return (Node*)obj_create(fmt("{:n}{obj_ext}", bmi));
}

static void add_compile_std_module_extra_options(Node* node, ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM)
    {
        c_compile_cmd_add_flag(node, "-Wno-reserved-module-identifier");
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            c_compile_cmd_add_flag(node, "-Wno-include-angled-in-module-purview");
        }
    }
}

Node* create_compile_std_module_for_compile_cmd(CCompileCmd* cmd, Node* src, Node* bmi, bool b_compat)
{
    Node* obj = obj_from_bmi(bmi);
    Node* node = c_compile_cmd_create(src, obj, __FILE__, __LINE__);
    c_compile_cmd_set_export(node, b_compat ? "std.compat" : "std", bmi);
    add_compile_std_module_extra_options(node, cmd->toolchain);
    CCompileCmd* compile_std_cmd = (CCompileCmd*)node;
    compile_std_cmd->toolchain = cmd->toolchain;
    compile_std_cmd->cpp_std = cmd->cpp_std;
    compile_std_cmd->optimization_type = cmd->optimization_type;
    compile_std_cmd->arch = cmd->arch;
    return node;
}

Node* get_or_create_std_module_for_compile_cmd(CCompileCmd* cmd)
{
    Node* src = get_std_module_source_node(cmd->toolchain);
    if (src == NULL)
    {
        return NULL;
    }
    char const* bmi_path = get_std_module_bmi_path(src->path, cmd->toolchain, cmd->arch, cmd->optimization_type, cmd->cpp_std);
    Node* bmi = find_node(bmi_path);
    if (bmi)
    {
        return bmi;
    }
    bmi = get_or_add_file(bmi_path);
    create_compile_std_module_for_compile_cmd(cmd, src, bmi, false);
    return bmi;
}

Node* get_or_create_std_compat_module_for_compile_cmd(CCompileCmd* cmd)
{
    Node* src = get_std_compat_module_source_node(cmd->toolchain);
    char const* bmi_path = get_std_module_bmi_path(src->path, cmd->toolchain, cmd->arch, cmd->optimization_type, cmd->cpp_std);
    Node* bmi = find_node(bmi_path);
    if (bmi)
    {
        return bmi;
    }
    bmi = get_or_add_file(bmi_path);
    create_compile_std_module_for_compile_cmd(cmd, src, bmi, false);
    return bmi;
}

Node* module_from_src(Node* src)
{
    Allocator* temp_allocator = allocator_temp();
    char const* ext = get_toolchain_bmi_extension(default_toolchain);
    char const* bmi_path = determine_imtermediate_path(src->path, "modules", ext, temp_allocator);
    return get_or_add_file(bmi_path);
}

Node* module_from_src_with_variant(Node* src, char const* variant)
{
    Allocator* temp_allocator = allocator_temp();
    char const* ext = get_toolchain_bmi_extension(default_toolchain);
    char const* bmi_path = determine_imtermediate_path(src->path, "modules", ext, temp_allocator);
    char const* stem = path_stem(bmi_path, allocator_temp());
    char const* parent_path = path_parent_path(bmi_path, allocator_temp());
    if (parent_path)
    {
        bmi_path = fmt("{}/{}{}{}", parent_path, stem, variant, ext);
    }
    else
    {
        bmi_path = fmt("{}{}{}", stem, variant, ext);
    }
    return get_or_add_file(bmi_path);
}


#include <assert.h>
#include <stdbool.h>

extern Allocator* node_allocator;

bool b_generate_compile_commands = true;
char const* compile_commands_json_path = NULL;

static char* gen_compile_commands_json_escape(char const* cstr, Allocator* allocator)
{
    char* result = string_from_c_str(allocator, "");
    for (uint64_t i = 0; cstr[i] != '\0'; i++)
    {
        if (cstr[i] == '\\')
        {
            string_printf(allocator, result, "\\\\");
        }
        else if (cstr[i] == '"')
        {
            string_printf(allocator, result, "\\\"");
        }
        else
        {
            string_printf(allocator, result, "%c", cstr[i]);
        }
    }
    return result;
}

static char* get_compile_commands_string(Node* cmd, Allocator* allocator)
{
    char* result = NULL;
    Allocator* temp_allocator = allocator_create_chained();
    char const* cwd = gen_compile_commands_json_escape(os_get_cwd(temp_allocator), temp_allocator);
    string_printf(allocator, result, "[");
    Node** nodes = cmd->dependencies;
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        assert(node->node_type == NODE_TYPE_VIRTUAL && node->virtual_ext_type == VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE);
        CompileCmdline* cmdline = (CompileCmdline*)node;
        CCompileCmd* compile_cmd = cmdline->cmd;
        char const* cmd_str = gen_compile_commands_json_escape(compile_cmd->cmdline, temp_allocator);
        static bool b_first = true;
        if (!b_first)
        {
            string_putc(allocator, result, ',');
        }
        else
        {
            b_first = false;
        }
        string_printf(allocator, result, "\n");
        string_printf(allocator, result, "    {\n");
        string_printf(allocator, result, "        \"directory\": \"%s\",\n", cwd);
        string_printf(allocator, result, "        \"command\": \"%s\",\n", cmd_str);
        string_printf(allocator, result, "        \"file\": \"%s\"\n", gen_compile_commands_json_escape(compile_cmd->src->path, temp_allocator));
        string_printf(allocator, result, "    }");
    }
    string_printf(allocator, result, "\n]");
    allocator_destroy(temp_allocator);
    return result;
}

static bool gen_compile_commands_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    char const* path = cmd->outputs[0]->path;
    Allocator* temp_allocator = allocator_create_chained();
    char* result = get_compile_commands_string(cmd, node_allocator);
    char* old_content = os_read_all(temp_allocator, path);
    bool b_dirty = false;
    if (old_content == NULL || !string_equal(old_content, result))
    {
        cmd->extra_data = result;
        b_dirty = true;
    }
    else
    {
        array_free(node_allocator, result);
    }
    allocator_destroy(temp_allocator);
    return b_dirty;
}

static int gen_compile_commands_thread_fn(Node* cmd)
{
    char const* path = cmd->outputs[0]->path;
    char const* compdb = NULL;
    if (cmd->extra_data)
    {
        compdb = cmd->extra_data;
    }
    else
    {
        compdb = get_compile_commands_string(cmd, node_allocator);
    }
    os_write_all(path, compdb, array_size(compdb));
    array_free(node_allocator, compdb);
    return EXIT_SUCCESS;
}

void set_generate_compile_commands_enabled(bool enabled)
{
    b_generate_compile_commands = enabled;
}

void set_compile_commands_json_path(char const* path)
{
    compile_commands_json_path = path;
}

ENTRY(gen_compile_commands_entry, PRIORITY_AFTER_DEFAULT)
{
    Node* output = get_or_add_file(compile_commands_json_path);
    output->b_default_excluded = !b_generate_compile_commands;
    Node* cmd = CALLBACK_CMD(gen_compile_commands_thread_fn, NULL);
    cmd->b_default_excluded = !b_generate_compile_commands;
    node_set_alias(cmd, "compdb");
    node_set_check_dirty_fn(cmd, gen_compile_commands_check_dirty);
    cmd_add_output(cmd, output);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{}{#}", output->path));

    Node** nodes = get_all_nodes();
    for (uint64_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_VIRTUAL || node->virtual_ext_type != VIRTUAL_EXT_TYPE_MAKE_COMPILE_CMDLINE)
        {
            continue;
        }
        node_add_dependency(cmd, node);
    }
}


#include <assert.h>
#include <stddef.h>
#include <stdint.h>

extern Allocator* node_allocator;
extern bool b_generate_debug_info;
extern ToolchainType default_toolchain;
extern ArchitectureType default_architecture_type;
extern OptimizationType default_optimization_type;
extern char* g_zig_target;
typedef struct LibOrderPair
{
    char const* lib;
    size_t index;
} LibOrderPair;

static int lib_order_pair_compare(void const* key, const void* element)
{
    LibOrderPair* k = (LibOrderPair*)key;
    LibOrderPair* e = (LibOrderPair*)element;
    if (k->index < e->index) return -1;
    else if (k->index == e->index) return 0;
    else return 1;
}

static void msvc_lib_output_filter(Node* cmd, char const* line)
{
    if (string_starts_with(line, "   Creating library"))
    {
        return;
    }
    cmd_write_stdout_line(cmd, line);
}

static Node* get_module_obj_from_bmi(Node* bmi)
{
    if (!bmi || !bmi->build_cmd)
    {
        return NULL;
    }

    if (bmi->build_cmd->cmd_ext_type == C_CMD_COMPILE)
    {
        return ((CCompileCmd*)bmi->build_cmd)->out_obj;
    }
    else if (bmi->build_cmd->cmd_ext_type == C_CMD_BMI_TO_OBJ)
    {
        return ((BmiToObjCmd*)bmi->build_cmd)->c_compile_cmd->out_obj;
    }

    return NULL;
}

static Node** collect_linked_node_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set);

static Node** collect_cpp_module_objs_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set)
{
    CCompileCmd* cc = obj_get_compile_cmd(input);
    if (!cc || !cc->b_cpp)
    {
        return nodes;
    }

    StringPtrHash h = {.allocator = allocator_temp()};
    c_compile_cmd_get_all_imports(cc, &h);

    for (uint32_t i = h.begin; i != h.end; i = hash_next(&h, i))
    {
        Node* bmi = hash_value(&h, i);
        Node* module_obj = get_module_obj_from_bmi(bmi);

        if (module_obj)
        {
            nodes = collect_linked_node_recursively(allocator, module_obj, nodes, node_set, lib_set);
        }
    }
    return nodes;
}

static Node** collect_linked_node_recursively(Allocator* allocator, Node* input, Node** nodes, StringSet* node_set, StringHash* lib_set)
{
    bool b_existed;
    hash_insert_check(node_set, input->path, &b_existed);
    if (b_existed)
    {
        return nodes;
    }
    array_push(allocator, nodes, input);
    if (input->file_type == FILE_TYPE_OBJ)
    {
        Obj* obj = (Obj*)input;
        for (size_t i = 0; i != array_size(obj->link_libs); i++)
        {
            uint32_t j = hash_insert_check(lib_set, obj->link_libs[i], &b_existed);
            if (!b_existed)
            {
                hash_value(lib_set, j) = lib_set->size - 1;
            }
        }
        for (size_t i = 0; i != array_size(obj->link_nodes); i++)
        {
            Node* other = obj->link_nodes[i];
            nodes = collect_linked_node_recursively(allocator, other, nodes, node_set, lib_set);
        }
        nodes = collect_cpp_module_objs_recursively(allocator, input, nodes, node_set, lib_set);
    }
    if (input->file_type == FILE_TYPE_LIB)
    {
        Node* lib = input;
        if (lib->build_cmd == NULL)
        {
            return nodes;
        }
        ArCmd* ar_cmd = (ArCmd*)lib->build_cmd;
        for (size_t i = 0; i != array_size(ar_cmd->inputs); i++)
        {
            Node* obj = ar_cmd->inputs[i];
            if (obj->file_type != FILE_TYPE_OBJ)
            {
                continue;
            }
            nodes = collect_linked_node_recursively(allocator, obj, nodes, node_set, lib_set);
        }
    }
    return nodes;
}

static void link_cmd_get_lib_obj_set(Node** files, Set* set)
{
    for (size_t i = 0; i != array_size(files); i++)
    {
        Node* input = files[i];
        if (input->file_type != FILE_TYPE_LIB)
        {
            continue;
        }
        if (input->build_cmd == NULL || input->build_cmd->cmd_ext_type != C_CMD_AR)
        {
            continue;
        }
        node_ensure_prepared(input);
        node_ensure_prepared(input->build_cmd);
        ArCmd* ar_cmd = (ArCmd*)input->build_cmd;
        for (size_t j = 0; j != array_size(ar_cmd->inputs); j++)
        {
            if (ar_cmd->inputs[j]->file_type == FILE_TYPE_OBJ)
            {
                hash_insert(set, (uintptr_t)ar_cmd->inputs[j]);
            }
        }
    }
}

static Node** link_cmd_collect_all_linked(LinkCmd* cmd, Allocator* temp_allocator, StringSet* node_set, StringHash* lib_set)
{
    Node** temp_files = NULL;
    for (size_t i = 0; i != array_size(cmd->input_option_files); i++)
    {
        Node* input = cmd->inputs[i];
        if (input->build_cmd == NULL)
        {
            continue;
        }
        node_ensure_prepared(input->build_cmd);
        temp_files = collect_linked_node_recursively(temp_allocator, input, temp_files, node_set, lib_set);
    }
    Set lib_objs = {.allocator = temp_allocator};
    for (size_t i = 0; i != array_size(temp_files); i++)
    {
        link_cmd_get_lib_obj_set(temp_files, &lib_objs);
    }
    Node** files = NULL;
    for (size_t i = 0; i != array_size(temp_files); i++)
    {
        Node* file = temp_files[i];
        if (!hash_has_key(&lib_objs, (uintptr_t)file))
        {
            array_push(temp_allocator, files, file);
        }
    }
    array_free(temp_allocator, temp_files);
    for (size_t i = 0; i != array_size(cmd->libs); i++)
    {
        bool b_existed;
        uint32_t j = hash_insert_check(lib_set, cmd->libs[i], &b_existed);
        if (!b_existed)
        {
            hash_value(lib_set, j) = lib_set->size - 1;
        }
    }
    return files;
}

static void link_cmd_add_link_lib_option(Node* node, char const* lib_name)
{
    LinkCmd* link = (LinkCmd*)node;
    extern char const* desc_color_input;
    extern char const* desc_color_reset;
    extern char const* desc_color_flag;

    if (link->linker_type == LINKER_LINK)
    {
        string_printf(node_allocator, node->cmdline, " %s.lib", lib_name);
        string_printf(node_allocator, node->description, " %s%s.lib%s", desc_color_flag, lib_name, desc_color_reset);
    }
    else
    {
        string_printf(node_allocator, node->cmdline, " -l%s", lib_name);
        string_printf(node_allocator, node->description, " %s-l%s%s%s%s", desc_color_flag, desc_color_reset, desc_color_input, lib_name, desc_color_reset);
    }
}
void link_cmd_add_input_options(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    Allocator* allocator = allocator_temp();
    StringSet node_set = {.allocator = allocator};
    StringHash lib_set = {.allocator = allocator};
    Node** all_inputs = link_cmd_collect_all_linked(link, allocator, &node_set, &lib_set);
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        if (input->file_type == FILE_TYPE_LIB)
        {
            continue;
        }
        cmd_add_input_file_option(cmd, NULL, input);
    }
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        if (input->file_type != FILE_TYPE_LIB)
        {
            continue;
        }
        cmd_add_input_file_option(cmd, NULL, input);
    }
    LibOrderPair* lib_order_pairs = NULL;
    for (uint32_t i = lib_set.begin; i != lib_set.end; i = hash_next(&lib_set, i))
    {
        char const* lib = hash_key(&lib_set, i);
        size_t index = hash_value(&lib_set, i);
        LibOrderPair p = {lib, index};
        array_push(allocator, lib_order_pairs, p);
    }
    if (lib_order_pairs)
    {
        size_t num_libs = array_size(lib_order_pairs);
        qsort(lib_order_pairs, num_libs, sizeof(LibOrderPair), lib_order_pair_compare);
    }
    for (size_t i = 0; i != array_size(lib_order_pairs); i++)
    {
        char const* lib = lib_order_pairs[i].lib;
        link_cmd_add_link_lib_option(cmd, lib);
    }
}

void link_cmd_add_all_linked_input(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    Allocator* allocator = allocator_temp();
    StringSet node_set = {.allocator = allocator};
    StringHash lib_set = {.allocator = allocator};
    Node** all_inputs = link_cmd_collect_all_linked(link, allocator, &node_set, &lib_set);
    for (size_t i = 0; i != array_size(all_inputs); i++)
    {
        Node* input = all_inputs[i];
        cmd_add_input(cmd, input);
    }
}

static bool link_cmd_can_output_pdb(LinkerType linker)
{
    if (linker == LINKER_LINK || linker == LINKER_LLVM_LINK)
    {
        return true;
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (linker == LINKER_LLVM_LD || linker == LINKER_LLVM_LLD))
    {
        return true;
    }
    return false;
}

Node* link_cmd_set_pdb_base_on_output(Node* node)
{
    LinkCmd* cmd = (LinkCmd*)node;
    if (!link_cmd_can_output_pdb(cmd->linker_type))
    {
        return NULL;
    }
#if CURRENT_PLATFORM != PLATFORM_WINDOWS
    return NULL;
#endif
    if (cmd->toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return NULL;
    }
    char const* pdb_path = fmt("{:n}.pdb", cmd->output);
    Node* pdb = get_or_add_file(pdb_path);
    link_cmd_set_pdb(node, pdb);
    return pdb;
}

static char const* link_cmd_get_out_option(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/out:";
    }
    if (linker_type == LINKER_LLVM_LINK || (CURRENT_PLATFORM == PLATFORM_WINDOWS && linker_type == LINKER_LLVM_LLD))
    {
        return "-Wl,/out:";
    }
    return "-o ";
}

static bool link_cmd_check_is_linking_cpp(LinkCmd* cmd)
{
    bool b_link_cpp_obj = false;
    for (size_t i = 0; i != array_size(cmd->inputs); i++)
    {
        Node* input = cmd->inputs[i];
        if (input->file_type != FILE_TYPE_OBJ)
        {
            continue;
        }
        if (!input->build_cmd || input->build_cmd->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        CCompileCmd* compile_cmd = (CCompileCmd*)input->build_cmd;
        if (compile_cmd->b_cpp)
        {
            b_link_cpp_obj = true;
            break;
        }
    }
    return b_link_cpp_obj;
}

static char const* link_cmd_get_linker_gcc_llvm_zig(LinkCmd* cmd, ToolchainType toolchain_type)
{
    if (toolchain_type == TOOLCHAIN_TYPE_GCC)
    {
        if (cmd->b_link_cpp)
        {
            return "g++";
        }
        else
        {
            return "gcc";
        }
    }
    if (toolchain_type == TOOLCHAIN_TYPE_LLVM)
    {
        if (cmd->b_link_cpp)
        {
            return "clang++";
        }
        else
        {
            return "clang";
        }
    }
    if (toolchain_type == TOOLCHAIN_TYPE_ZIG)
    {
        if (cmd->b_link_cpp)
        {
            return "zig c++";
        }
        else
        {
            return "zig cc";
        }
    }
    assert(false);
    return NULL;
}

static char const* link_cmd_get_linker(LinkCmd* cmd)
{
    if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (cmd->linker_type == LINKER_LINK)
        {
            return "link";
        }
        if (cmd->linker_type == LINKER_LLVM_LD || cmd->linker_type == LINKER_LLVM_LLD || cmd->linker_type == LINKER_LLVM_LINK)
        {
            return link_cmd_get_linker_gcc_llvm_zig(cmd, cmd->toolchain);
        }
        assert(false && "Unknown linker");
    }
    else if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return "link";
    }
    else if (cmd->toolchain == TOOLCHAIN_TYPE_GCC || cmd->toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        return link_cmd_get_linker_gcc_llvm_zig(cmd, cmd->toolchain);
    }
    assert(false);
    return NULL;
}

static char const* link_cmd_get_option_shared(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/dll";
    }
    return "-shared";
}

static char const* link_cmd_get_option_pdb(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/pdb:";
    }
    if (linker_type == LINKER_LLVM_LINK || (CURRENT_PLATFORM == PLATFORM_WINDOWS && linker_type == LINKER_LLVM_LLD))
    {
        return "-Wl,/pdb:";
    }
    return NULL;
}

static char const* link_cmd_get_option_def(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/def:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && linker_type == LINKER_LD)
    {
        return "";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        return "-Wl,/def:";
    }
    return NULL;
}

static char const* link_cmd_get_option_out_import_lib(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/implib:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (linker_type == LINKER_LLVM_LLD || linker_type == LINKER_LLVM_LINK)
        {
            return "-Wl,/implib:";
        }
        return "--out-implib ";
    }
    else
    {
        return NULL;
    }
}

static char const* link_cmd_get_option_entry(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/entry:";
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (linker_type == LINKER_LLVM_LLD || linker_type == LINKER_LLVM_LINK))
    {
        return "-Wl,/entry:";
    }
    if (linker_type == LINKER_LLVM_LD)
    {
        return "-Wl,-e ";
    }
    return "-e ";
}

static char* link_cmd_get_default_options_msvc_llvm_common(LinkCmd* link, Allocator* allocator, char* options)
{
    if (link->out_import_lib == NULL)
    {
        string_concat_c_str(allocator, options, " /noimplib");
    }
    return options;
}

static char const* link_cmd_get_default_options_llvm(LinkCmd* link, Allocator* allocator)
{
    char* options = NULL;
    if (link->b_link_cpp && CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        string_concat_c_str(allocator, options, " -stdlib=libc++");
    }
    if (link->linker_type == LINKER_LLVM_LD)
    {
        string_concat_c_str(allocator, options, " -fuse-ld=ld");
    }
    if (link->linker_type == LINKER_LLVM_LINK)
    {
        string_concat_c_str(allocator, options, " -fuse-ld=link");
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && (link->linker_type == LINKER_LLVM_LLD || link->linker_type == LINKER_LLVM_LINK))
    {
        if (link->out_import_lib == NULL)
        {
            string_concat_c_str(allocator, options, " -Wl,/noimplib");
        }
    }
    return options;
}

static char const* link_cmd_get_default_options_msvc(LinkCmd* link, Allocator* allocator)
{
    char* options = NULL;
    options = link_cmd_get_default_options_msvc_llvm_common(link, allocator, options);
    string_concat_c_str(allocator, options, " /nologo /incremental:no");
    string_concat_c_str(allocator, options, " /noexp");
    return options;
}

static char const* link_cmd_get_default_options(LinkCmd* link, Allocator* allocator)
{
    if (link->toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        return link_cmd_get_default_options_llvm(link, allocator);
    }
    if (link->toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return link_cmd_get_default_options_msvc(link, allocator);
    }
    return NULL;
}

static char const* link_cmd_get_option_arch(ToolchainType toolchain, ArchitectureType arch)
{
    if (toolchain != TOOLCHAIN_TYPE_MSVC)
    {
        if (arch == ARCH_X64)
        {
            return "-m64";
        }
        if (arch == ARCH_X86)
        {
            return "-m32";
        }
    }
    return NULL;
}

static char const* link_cmd_get_option_debug(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/debug";
    }
    return "-g";
}

static char const* link_cmd_get_option_lib_dir(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return "/libpath:";
    }
    return "-L";
}

static char const* link_cmd_get_option_lib(LinkerType linker_type)
{
    if (linker_type == LINKER_LINK)
    {
        return NULL;
    }
    else
    {
        return "-l";
    }
}

static LinkerType link_cmd_get_default_linker_type(ToolchainType toolchain)
{
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        return get_llvm_linker_type();
    }
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        return LINKER_LINK;
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        return LINKER_LD;
    }
    if (toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        return LINKER_ZIG_CC;
    }
    return LINKER_UNSPECIFIED;
}

void link_cmd_make_cmdline(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    array_resize(node_allocator, link->cmdline, 0);
    array_resize(node_allocator, link->description, 0);
    char const* opt_out = link_cmd_get_out_option(link->linker_type);
    char const* linker = link_cmd_get_linker(link);
    cmd_add_option(cmd, NULL, linker, OPTION_EXE);
    cmd_add_option(cmd, opt_out, link->output->path, OPTION_OUTPUT);
    if (link->output->file_type == FILE_TYPE_DLL)
    {
        char const* opt_shared = link_cmd_get_option_shared(link->linker_type);
        cmd_add_option(cmd, opt_shared, NULL, OPTION_FLAG);
    }
    link_cmd_add_input_options(cmd);
    if (link->pdb)
    {
        char const* opt_pdb = link_cmd_get_option_pdb(link->linker_type);
        if (opt_pdb)
        {
            cmd_add_option(cmd, opt_pdb, link->pdb->path, OPTION_OUTPUT);
        }
    }
    if (link->def)
    {
        char const* opt_def = link_cmd_get_option_def(link->linker_type);
        if (opt_def)
        {
            cmd_add_option(cmd, opt_def, link->def->path, OPTION_INPUT);
        }
    }
    char const* opt_out_import_lib = link_cmd_get_option_out_import_lib(link->linker_type);
    if (link->out_import_lib && opt_out_import_lib)
    {
        cmd_add_option(cmd, opt_out_import_lib, link->out_import_lib->path, OPTION_OUTPUT);
    }
    if (link->entry)
    {
        char const* opt_entry = link_cmd_get_option_entry(link->linker_type);
        cmd_add_option(cmd, opt_entry, link->entry, OPTION_FLAG);
    }
    char const* opt_default = link_cmd_get_default_options(link, allocator_temp());
    if (opt_default)
    {
        cmd_add_option(cmd, opt_default + 1, NULL, OPTION_FLAG);
    }
    if (link->toolchain != TOOLCHAIN_TYPE_MSVC)
    {
        char const* opt_arch = link_cmd_get_option_arch(link->toolchain, link->arch);
        if (opt_arch)
        {
            cmd_add_option(cmd, opt_arch, NULL, OPTION_FLAG);
        }
    }
    if (link->pdb)
    {
        char const* opt_debug = link_cmd_get_option_debug(link->linker_type);
        cmd_add_option(cmd, opt_debug, NULL, OPTION_FLAG);
    }
    for (size_t i = 0; i != array_size(link->flags); i++)
    {
        cmd_add_option(cmd, link->flags[i], NULL, OPTION_FLAG);
    }
    char const* opt_lib_dir = link_cmd_get_option_lib_dir(link->linker_type);
    for (size_t i = 0; i != array_size(link->lib_directories); i++)
    {
        cmd_add_option(cmd, opt_lib_dir, link->lib_directories[i], OPTION_FLAG);
    }

    if (link->toolchain == TOOLCHAIN_TYPE_ZIG && g_zig_target)
    {
        cmd_add_option(cmd, "-target ", g_zig_target, OPTION_FLAG);
    }
}

static void link_cmd_prepare(Node* node)
{
    LinkCmd* link = (LinkCmd*)node;
    link->b_link_cpp = link_cmd_check_is_linking_cpp(link);
    if (b_generate_debug_info)
    {
        if (link->pdb == NULL)
        {
            link_cmd_set_pdb_base_on_output(node);
        }
        if (link->pdb)
        {
            cmd_add_output(node, link->pdb);
        }
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS && link->def)
    {
        char const* path = path_replace_extension(link->output->path, LIB_EXT, allocator_temp());
        Node* import_lib = get_or_add_file(path);
        make_implib_cmd_create(import_lib, link->def, link->toolchain, link->arch, __FILE__, __LINE__);
        cmd_add_input(node, link->def);
    }
    Node* env = get_toolchain_env_node(link->toolchain, link->arch);
    if (env)
    {
        cmd_set_env(node, env);
    }
    link_cmd_add_all_linked_input(node);

    cmd_prepare(node);
}

static void link_cmd_before_execute(Node* node)
{
    void c_toolchain_rename_to_old(char const* path);

    LinkCmd* link = (LinkCmd*)node;
    if (!os_file_writable(link->output->path))
    {
        c_toolchain_rename_to_old(link->output->path);
    }
    if (link->pdb && !os_file_writable(link->pdb->path))
    {
        c_toolchain_rename_to_old(link->pdb->path);
    }
    cmd_before_execute(node);
}

static bool link_cmd_check_dirty(Node* node)
{
    link_cmd_make_cmdline(node);
    return cmd_check_dirty(node);
}

Node* link_cmd_create(Node* output, char const* file, int line)
{
    assert(output->build_cmd == NULL);
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_LINK);
    Node* cmd = node_create(node_type, fmt("link: {:n}", output), sizeof(LinkCmd));
    LinkCmd* link = (LinkCmd*)cmd;
    link->toolchain = default_toolchain;
    link->output = output;
    link->arch = default_architecture_type;
    link->prepare = link_cmd_prepare;
    link->before_execute = link_cmd_before_execute;
    link->check_dirty = link_cmd_check_dirty;
    link->linker_type = link_cmd_get_default_linker_type(link->toolchain);
    link->optimization = default_optimization_type;
    cmd_set_source_location(cmd, file, line);
    cmd_add_output(cmd, output);
    node_set_extra_data(cmd, link);

    return cmd;
}

void link_cmd_add_input(Node* cmd, Node* file)
{
    LinkCmd* link_cmd = (LinkCmd*)cmd;
    array_push(node_allocator, link_cmd->input_option_files, file);
    cmd_add_input(cmd, file);
}

void link_cmd_set_pdb(Node* cmd, Node* pdb)
{
    LinkCmd* link = (LinkCmd*)cmd;
    if (!link_cmd_can_output_pdb(link->linker_type))
    {
        return;
    }
    link->pdb = pdb;
}

void link_cmd_set_out_import_lib(Node* cmd, Node* out_import_lib)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->out_import_lib = out_import_lib;
}

Node* link_cmd_get_out_import_lib(Node* cmd)
{
    LinkCmd* link = (LinkCmd*)cmd;
    return link->out_import_lib;
}

void link_cmd_set_def_file(Node* cmd, Node* def)
{
    if (CURRENT_PLATFORM != PLATFORM_WINDOWS)
    {
        return;
    }
    LinkCmd* link = (LinkCmd*)cmd;
    link->def = def;
}

void link_cmd_add_lib(Node* cmd, char const* lib)
{
    LinkCmd* link = (LinkCmd*)cmd;
    lib = string_from_c_str(node_allocator, lib);
    array_push(node_allocator, link->libs, lib);
}

void link_cmd_add_lib_dir(Node* cmd, char const* dir)
{
    LinkCmd* link = (LinkCmd*)cmd;
    dir = string_from_c_str(node_allocator, dir);
    array_push(node_allocator, link->lib_directories, dir);
}

void link_cmd_add_flag(Node* cmd, char const* flag)
{
    LinkCmd* link = (LinkCmd*)cmd;
    flag = string_from_c_str(node_allocator, flag);
    array_push(node_allocator, link->flags, flag);
}

void link_cmd_set_entry(Node* cmd, char const* name)
{
    LinkCmd* link = (LinkCmd*)cmd;
    array_resize(node_allocator, link->entry, 0);
    string_concat_c_str(node_allocator, link->entry, name);
}

void link_cmd_set_arch(Node* cmd, ArchitectureType arch)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->arch = arch;
}

void link_cmd_set_toolchain_type(Node* cmd, ToolchainType toolchain_type)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->toolchain = toolchain_type;
}

void link_cmd_set_linker_type(Node* cmd, LinkerType linker_type)
{
    LinkCmd* link = (LinkCmd*)cmd;
    link->linker_type = linker_type;
}

void link_cmd_setup_self_build(Node* cmd)
{
    extern ToolchainType self_build_toolchain;
    link_cmd_set_toolchain_type(cmd, self_build_toolchain);
    link_cmd_set_linker_type(cmd, link_cmd_get_default_linker_type(self_build_toolchain));
}


extern Allocator* node_allocator;

Node** scan_deps_cmds = NULL;

static StringPtrHash* get_default_module_mapper()
{
    static StringPtrHash* module_mapper = NULL;
    if (module_mapper == NULL)
    {
        module_mapper = allocator_calloc(node_allocator, 1, sizeof(StringPtrHash));
        module_mapper->allocator = node_allocator;
    }
    return module_mapper;
}

void compile_cmdline_node_make_cmdline_llvm_gcc_common(Node* node, CCompileCmd* cmd);
void compile_cmdline_node_make_cmdline_msvc_scan_deps_common(Node* node, CCompileCmd* cmd);
char const* c_compile_cmd_get_depfile_path(CCompileCmd* cmd);
void cmd_after_execute_parse_depfile(Node* node);
void c_compile_cmd_write_buffer_msvc(Node* node, char const* line);

void scan_deps_cmd_add_option_mm_mf(Node* node, ScanDepsCmd* cmd)
{
    char const* depfile_path = c_compile_cmd_get_depfile_path(cmd->compile_cmd);
    cmd_add_option(node, "-MM -MF ", depfile_path, OPTION_FLAG);
}

static void scan_deps_cmd_make_cmdline_llvm(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, NULL, "clang-scan-deps", OPTION_EXE);
    cmd_add_option(node, "--format ", "p1689", OPTION_FLAG);
    cmd_add_option(node, "-- ", "clang++", OPTION_FLAG);
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd->compile_cmd);
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        scan_deps_cmd_add_option_mm_mf(node, cmd);
    }
}

static void scan_deps_cmd_make_cmdline_msvc(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, NULL, "cl", OPTION_EXE);
    cmd_add_option(node, "/scanDependencies-", NULL, OPTION_FLAG);
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        cmd_add_option(node, "/showIncludes", NULL, OPTION_FLAG);
    }
    compile_cmdline_node_make_cmdline_msvc_scan_deps_common(node, cmd->compile_cmd);
}

static void scan_deps_cmd_make_cmdline_gcc(Node* node, ScanDepsCmd* cmd)
{
    array_resize(node_allocator, node->cmdline, 0);
    array_resize(node_allocator, node->description, 0);
    cmd_add_option(node, NULL, "g++", OPTION_EXE);
    cmd_add_option(node, "-fmodules", NULL, OPTION_FLAG);
    cmd_add_option(node, "-E", NULL, OPTION_FLAG);
    cmd_add_option(node, "-fdirectives-only", NULL, OPTION_FLAG);
    cmd_add_option(node, "-fdeps-format=", "p1689r5", OPTION_FLAG);
    cmd_add_option(node, "-fdeps-file=", "-", OPTION_FLAG);
    cmd_add_option(node, "-MM", NULL, OPTION_FLAG);
    cmd_add_option(node, "-MG", NULL, OPTION_FLAG);
    compile_cmdline_node_make_cmdline_llvm_gcc_common(node, cmd->compile_cmd);
    if (cmd->compile_cmd->b_cache_header_dependencies)
    {
        scan_deps_cmd_add_option_mm_mf(node, cmd);
    }
}

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);

static bool scan_deps_cmd_read_p1689(ScanDepsCmd* cmd)
{
    Allocator* temp_allocator = allocator_temp();
    JsonValue json = json_from_string(cmd->std_output, temp_allocator);
    if (json.type != JSON_TYPE_OBJECT)
    {
        return false;
    }
    JsonValue* rules = json_object_get_value(&json.object, "rules");
    if (rules == NULL)
    {
        return false;
    }
    if (array_size(rules->array) == 0)
    {
        return false;
    }
    cmd->export_name = NULL;
    cmd->imports = NULL;
    JsonValue* rules0 = &rules->array[0];
    JsonValue* provides = json_object_get_value(&rules0->object, "provides");
    if (provides)
    {
        JsonValue* provides1 = &provides->array[0];
        JsonValue* logical_name = json_object_get_value(&provides1->object, "logical-name");
        cmd->export_name = string_from_c_str(node_allocator, logical_name->string);
    }

    JsonValue* _requires = json_object_get_value(&rules0->object, "requires");
    if (_requires)
    {
        size_t num_requires = array_size(_requires->array);
        for (size_t i = 0; i != num_requires; i++)
        {
            JsonValue* req = &_requires->array[i];
            JsonValue* req_name = json_object_get_value(&req->object, "logical-name");
            char* import_name = string_from_c_str(node_allocator, req_name->string);
            array_push(node_allocator, cmd->imports, import_name);
        }
    }
    return true;
}

static void scan_deps_cmd_update_cache(ScanDepsCmd* cmd)
{
    Cache* cache = get_cache();
    if (cache == NULL || (cmd->imports == NULL && cmd->export_name == NULL))
    {
        return;
    }
    Node* src = cmd->compile_cmd->src;
    CacheRecordCppModule* record = cache_find_cpp_module_record(cache, src->path);
    if (!record)
    {
        goto WriteNew;
    }
    size_t num_record_entries = array_size(record->imports);
    if (num_record_entries != array_size(cmd->imports))
    {
        goto WriteNew;
    }
    for (size_t i = 0; i != num_record_entries; i++)
    {
        if (!string_equal(record->imports[i], cmd->imports[i]))
        {
            goto WriteNew;
        }
    }
    if (array_size(cmd->export_name) != array_size(record->export))
    {
        goto WriteNew;
    }
    if (memcmp(cmd->export_name, record->export, array_size(record->export)) != 0)
    {
        goto WriteNew;
    }
    return;
WriteNew:

    CacheRecordFile* src_record = cache_get_or_add_in_file_record(cache, src->path);
    CacheRecordCppModule new_record = {
        .source_id = src_record->id,
        .export = cmd->export_name,
        .imports = cmd->imports,
    };
    cache_write_cpp_module_record(cache, &new_record);
}

static void scan_deps_cmd_after_execute(Node* node)
{
    ScanDepsCmd* cmd = (ScanDepsCmd*)node;
    if (!scan_deps_cmd_read_p1689(cmd))
    {
        warn("%s failed!", fmt("{:n}", cmd));
    }
    array_free(node_allocator, cmd->std_output);
    ToolchainType toolchain = cmd->compile_cmd->toolchain;
    if (toolchain == TOOLCHAIN_TYPE_LLVM || toolchain == TOOLCHAIN_TYPE_GCC)
    {
        cmd_after_execute_parse_depfile(node);
    }
    cmd_update_output_mtime(node);
    scan_deps_cmd_update_cache(cmd);
    cmd_after_execute(node);
}

static void scan_deps_cmd_before_execute_llvm_gcc(Node* node)
{
    cmd_before_execute(node);
    char const* depfile = node->ctx;
    os_ensure_dir_existed(depfile);
}

void scan_deps_cmd_llvm_gcc_setup_execute_callback(Node* node, ScanDepsCmd* cmd)
{
    node->after_execute = scan_deps_cmd_after_execute;
    CCompileCmd* compile_cmd = cmd->compile_cmd;
    node->ctx = string_from_print(node_allocator, "%s.d", compile_cmd->out_obj->path);
    node->before_execute = scan_deps_cmd_before_execute_llvm_gcc;
}

Node* scan_deps_cmd_create(CCompileCmd* compile_cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_SCAN_DEPS);
    char const* name = fmt("scan deps: {:n}", compile_cmd->src);
    Node* node = node_create(type, name, sizeof(ScanDepsCmd));
    array_push(node_allocator, scan_deps_cmds, node);
    ScanDepsCmd* cmd = (ScanDepsCmd*)node;
    cmd->compile_cmd = compile_cmd;
    compile_cmd->scan_deps_cmd = cmd;
    compile_cmd->export_map = get_default_module_mapper();
    compile_cmd->import_map = get_default_module_mapper();
    Node* test_h = FILE("{test_src_dir}/cup/test.h");
    if (has_dependency((Node*)cmd->compile_cmd, test_h))
    {
        node_add_dependency(node, test_h);
    }
    cmd_add_input(node, compile_cmd->src);
    cmd_set_source_location(node, compile_cmd->file, compile_cmd->line);
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordCppModule* record = cache_find_cpp_module_record(cache, compile_cmd->src->path);
        if (record)
        {
            cmd->export_name = record->export;
            cmd->imports = record->imports;
        }
    }
    switch (compile_cmd->toolchain)
    {
    case TOOLCHAIN_TYPE_MSVC:
        void c_compile_cmd_init_msvc(CCompileCmd * cmd);
        c_compile_cmd_init_msvc(compile_cmd);
        cmd_set_env(node, msvc_get_env_node(compile_cmd->toolchain, compile_cmd->arch));
        node->ctx = compile_cmd;
        node->write_stderr_line_fn = c_compile_cmd_write_buffer_msvc;
        scan_deps_cmd_make_cmdline_msvc(node, cmd);
        node->after_execute = scan_deps_cmd_after_execute;
        break;
    case TOOLCHAIN_TYPE_LLVM:
        scan_deps_cmd_make_cmdline_llvm(node, cmd);
        scan_deps_cmd_llvm_gcc_setup_execute_callback(node, cmd);
        break;
    case TOOLCHAIN_TYPE_GCC:
        scan_deps_cmd_make_cmdline_gcc(node, cmd);
        scan_deps_cmd_llvm_gcc_setup_execute_callback(node, cmd);
        break;
    case TOOLCHAIN_TYPE_ZIG:
        error("scan_deps_cmd_create: zig c++ do not support c++ modules");
        exit(EXIT_FAILURE);
    default:
        error("scan_deps_cmd_create: unknown toolchain");
        exit(EXIT_FAILURE);
    }
    return node;
}

typedef struct ScanTestCmd ScanTestCmd;

static int scan_test_cmd_thread_fn(Node* node)
{
    char** test_finder_get_entries(Allocator * allocator, char const* src_path);
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    // node_allocator cannot be used in a thread
    cmd->entries = test_finder_get_entries(allocator_c(), cmd->src->path);
    return 0;
}

static void scan_test_update_cache(ScanTestCmd* cmd)
{
    Cache* cache = get_cache();
    if (cache == NULL || cmd->entries == NULL)
    {
        return;
    }
    CacheRecordTestExe* record = cache_find_test_exe(cache, cmd->compile_cmd->src->path);
    if (!record)
    {
        goto WriteNew;
    }
    size_t num_record_entries = array_size(record->entries);
    if (num_record_entries != array_size(cmd->entries))
    {
        goto WriteNew;
    }
    for (size_t i = 0; i != num_record_entries; i++)
    {
        if (!string_equal(record->entries[i], cmd->entries[i]))
        {
            goto WriteNew;
        }
    }
    return;
WriteNew:
    CacheRecordFile* src_record = cache_get_or_add_in_file_record(cache, cmd->src->path);
    CacheRecordTestExe new_record = {
        .source_id = src_record->id,
        .entries = cmd->entries,
    };
    cache_write_test_exe_record(cache, &new_record);
}

extern Allocator* node_allocator;

static void scan_test_after_execute(Node* node)
{
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    Node* src = cmd->src;
    scan_test_update_cache(cmd);
    src->test_entries = (char const**)utilities_copy_string_array(node_allocator, (char const**)cmd->entries);
    array_free(allocator_c(), cmd->entries);
    cmd->entries = NULL;
    cmd_after_execute(node);
}

Node* scan_test_cmd_create(CCompileCmd* compile_cmd)
{
    uint32_t type = node_make_cmd_type(CMD_TYPE_THREAD, C_CMD_SCAN_TESTS);
    char const* name = fmt("scan_test: {:n}", compile_cmd->src);
    Node* node = find_node(name);
    if (node)
    {
        return node;
    }
    node = node_create(type, name, sizeof(ScanTestCmd));
    node->fn = scan_test_cmd_thread_fn;
    node->after_execute = scan_test_after_execute;
    node->b_default_excluded = true;
    ScanTestCmd* cmd = (ScanTestCmd*)node;
    cmd->src = compile_cmd->src;
    cmd->compile_cmd = compile_cmd;
    cmd_add_input(node, compile_cmd->src);
    Cache* cache = get_cache();
    if (cache)
    {
        CacheRecordTestExe* test_record = cache_find_test_exe(cache, cmd->src->path);
        if (test_record)
        {
            cmd->src->test_entries = (char const**)test_record->entries;
        }
    }
    return node;
}

void get_scan_test_cmds(Allocator* allocator, Node*** out_cmds)
{
    extern Node** nodes;
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_CMD ||
            node->cmd_type != CMD_TYPE_EXECUTABLE ||
            node->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        CCompileCmd* cc = (CCompileCmd*)node;
        if (cc->src->build_cmd)
        {
            continue;
        }
        Node* scan = scan_test_cmd_create(cc);
        node_ensure_prepared(scan);
        array_push(allocator, *out_cmds, scan);
    }
}

#if CURRENT_PLATFORM == PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

void* allocator_virtual_alloc(void* base_address, size_t size)
{
    return VirtualAlloc(base_address, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void allocator_virtual_free(void* base_address, size_t size)
{
    if (VirtualFree(base_address, 0, MEM_RELEASE) == 0)
    {
        printf("VirtualFree failed with error code %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define FIND_PATH_POSTFIX "\\*"

typedef struct Directory
{
    Allocator* allocator;
    HANDLE handle;
    WIN32_FIND_DATAW find_data;
    DirectoryEntry current_entry;
    char* path;
} Directory;

Directory* directory_open(const char* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_temp();
    size_t path_len = strlen(path);
    size_t buffer_size = path_len + sizeof(FIND_PATH_POSTFIX);
    char* buffer = NULL;
    array_resize(temp_allocator, buffer, buffer_size);
    strcpy(buffer, path);
    strcat(buffer, FIND_PATH_POSTFIX);
    wchar_t* wpath = utf8_to_wchars(temp_allocator, buffer);

    Directory* dir = allocator_malloc(allocator, sizeof(Directory));
    dir->allocator = allocator;
    dir->handle = FindFirstFileW(wpath, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        allocator_free(allocator, dir);
        return NULL;
    }
    else
    {
        dir->path = string_from_c_str(allocator, path);
        dir->current_entry.name = string_from_c_str(allocator, "");
        return dir;
    }
}

DirectoryEntry* directory_read(Directory* dir)
{
    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }
    else
    {
        dir->current_entry.is_directory = (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        array_resize(dir->allocator, dir->current_entry.name, 0);
        Allocator* stack_allocator = allocator_arena_from_alloca(4096);
        char const* name = wchars_to_utf8(stack_allocator, dir->find_data.cFileName);
        string_append(dir->allocator, dir->current_entry.name, name);
        if (!FindNextFileW(dir->handle, &dir->find_data))
        {
            FindClose(dir->handle);
            dir->handle = INVALID_HANDLE_VALUE;
        }
        return &dir->current_entry;
    }
}

void directory_close(Directory* dir)
{
    array_free(dir->allocator, dir->current_entry.name);
    array_free(dir->allocator, dir->path);
    if (dir->handle != INVALID_HANDLE_VALUE)
    {
        FindClose(dir->handle);
    }
    allocator_free(dir->allocator, dir);
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdint.h>
#include <stdio.h>

typedef struct Dylib Dylib;

Dylib* dylib_load(char const* name)
{
    if (name == NULL)
    {
        return (Dylib*)GetModuleHandle(NULL);
    }
    else
    {
        Dylib* lib = (Dylib*)LoadLibraryA(name);
        if (lib == NULL)
        {
            char* messageBuffer = NULL;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&messageBuffer,
                0,
                NULL);
            if (messageBuffer)
            {
                fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, messageBuffer);
                fprintf(stderr, "%s\n", name);
                LocalFree(messageBuffer);
            }
        }
        return lib;
    }
}

void dylib_unload(Dylib* plugin)
{
    if ((HANDLE)plugin != GetModuleHandle(NULL))
    {
        FreeLibrary((HANDLE)plugin);
    }
}

void* dylib_get_symbol(Dylib* lib, char const* name)
{
    return (void*)GetProcAddress((HMODULE)lib, name);
}

void* dylib_get_image_base(Dylib* lib)
{
    return lib;
}
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>



#include <bcrypt.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <userenv.h>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Userenv.lib")

uint64_t os_get_mtime(char const* path)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(temp_allocator, path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &data))
    {
        FILETIME ft_modify = data.ftLastWriteTime;
        return (uint64_t)ft_modify.dwLowDateTime | (uint64_t)ft_modify.dwHighDateTime << 32;
    }
    return 0;
}

bool os_file_exists(char const* path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

char* os_get_env(Allocator* allocator, const char* name)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wname = utf8_to_wchars(temp_allocator, name);
    wchar_t value[32768];
    if (!GetEnvironmentVariableW(wname, value, sizeof(value)))
    {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        {
            return NULL;
        }
    }
    return wchars_to_utf8(allocator, value);
}

void os_set_env(char const* name, char const* env)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wname = utf8_to_wchars(temp_allocator, name);
    wchar_t* w_env = env ? utf8_to_wchars(temp_allocator, env) : NULL;
    if (SetEnvironmentVariableW(wname, w_env) == 0)
    {
        fprintf(stderr, "Failed to set environment variable: %s\n", name);
    }
}

wchar_t* os_get_env_block(Allocator* allocator)
{
    wchar_t* env = GetEnvironmentStringsW();
    wchar_t* copy = NULL;
    for (wchar_t* p = env; *p != 0 || *(p + 1) != 0; p++)
    {
        array_push(allocator, copy, *p);
    }
    FreeEnvironmentStringsW(env);
    array_push(allocator, copy, L'\0');
    array_push(allocator, copy, L'\0');
    return copy;
}

wchar_t* os_get_default_env(void)
{
    static wchar_t* default_env = NULL;

    if (!default_env)
    {
        CreateEnvironmentBlock((LPVOID*)&default_env, NULL, FALSE);
        SetEnvironmentStringsW(default_env);
    }
    return default_env;
}

void os_reset_env(void)
{
    wchar_t* default_env = os_get_default_env();
    SetEnvironmentStringsW(default_env);
}

#define MKDIR(path) _mkdir(path)

char* os_get_cwd(Allocator* allocator)
{
    char* p = _getcwd(NULL, 0);
    char* cwd = string_from_c_str(allocator, p);
    free(p);
    return cwd;
}

bool os_set_cwd(char const* path)
{
    return _chdir(path) == 0;
}

char const* os_get_cmdline(void)
{
    static char* utf8_cmdline = NULL;
    if (!utf8_cmdline)
    {
        wchar_t* wcmd = GetCommandLineW();
        utf8_cmdline = wchars_to_utf8(allocator_c(), wcmd);
    }
    return utf8_cmdline;
}

bool os_remove_file(char const* path)
{
    Allocator* allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(allocator, path);
    bool b = DeleteFileW(wpath);
    return b;
}

char* os_get_current_exe_path(Allocator* allocator)
{
    wchar_t temp[4096];
    GetModuleFileNameW(NULL, temp, 4096);
    char* path = wchars_to_utf8(allocator, temp);
    path_backslash_to_slash(path);
    return path;
}

bool os_rename(char const* old_path, char const* new_path)
{
    Allocator* allocator = allocator_temp();
    wchar_t* src_w = utf8_to_wchars(allocator, old_path);
    wchar_t* dst_w = utf8_to_wchars(allocator, new_path);
    bool success = MoveFileExW(src_w, dst_w, MOVEFILE_REPLACE_EXISTING);
    return success;
}

bool os_copy_file(char const* src, char const* dst)
{
    Allocator* allocator = allocator_temp();
    wchar_t* src_w = utf8_to_wchars(allocator, src);
    wchar_t* dst_w = utf8_to_wchars(allocator, dst);
    bool success = CopyFileW(src_w, dst_w, false);
    return success;
}

bool os_mkdir(char const* path)
{
    return _mkdir(path) == 0;
}

uint64_t os_get_rand_uint64()
{
    uint64_t result;
    BCryptGenRandom(NULL, (PBYTE)&result, sizeof(uint64_t), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return result;
}

int os_get_cpu_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

FILE* os_fopen(char const* path, char const* mode)
{
    Allocator* allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(allocator, path);
    wchar_t* wmode = utf8_to_wchars(allocator, mode);
    return _wfopen(wpath, wmode);
}

FILE* os_popen(char const* cmd, char const* mode)
{
    return _popen(cmd, mode);
}

int os_pclose(FILE* file)
{
    return _pclose(file);
}

Process* os_start_process(char const* cmd)
{
    Allocator* temp_allocator = allocator_create_chained();
    STARTUPINFOW si = {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {0};
    wchar_t* cwd = NULL;
    wchar_t* cmd_w = utf8_to_wchars(temp_allocator, cmd);
    if (!CreateProcessW(NULL, cmd_w, NULL, NULL, FALSE, 0, NULL, cwd, &si, &pi))
    {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND)
        {
            fprintf(stderr, "error: os_start_process: CreateProcess failed");
        }
    }
    else
    {
        CloseHandle(pi.hThread);
    }
    allocator_destroy(temp_allocator);
    return pi.hProcess;
}

int os_wait_process(Process* p)
{
    WaitForSingleObject(p, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(p, &exit_code);
    return (int)exit_code;
}

void os_forget_process(Process* p)
{
    CloseHandle(p);
}

uint64_t os_get_file_size(char const* path)
{
    Allocator* temp_allocator = allocator_temp();
    wchar_t* wpath = utf8_to_wchars(temp_allocator, path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &data))
    {
        return (uint64_t)data.nFileSizeLow | (uint64_t)data.nFileSizeHigh << 32;
    }
    return UINT64_MAX;
}

char* os_full_path(char const* path, Allocator* allocator)
{
    wchar_t* wpath = utf8_to_wchars(allocator_temp(), path);
    wchar_t buffer[4096];
    GetFullPathNameW(wpath, 4096, buffer, NULL);
    char* result = wchars_to_utf8(allocator, buffer);
    path_backslash_to_slash(result);
    return result;
}

typedef struct LockFileContextWindows
{
    FILE* file;
    HANDLE handle;
    Allocator* allocator;
    OVERLAPPED overlapped;
} LockFileContextWindows;

LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared)
{
    os_ensure_dir_existed(path);
    HANDLE handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }
    LockFileContextWindows* ctx = allocator_calloc(allocator, 1, sizeof(LockFileContextWindows));
    ctx->handle = handle;
    DWORD lock_flags = b_shared ? 0 : LOCKFILE_EXCLUSIVE_LOCK;
    BOOL lock_result = LockFileEx(
        handle,
        lock_flags,
        0,
        MAXDWORD,
        MAXDWORD,
        &ctx->overlapped);
    if (!lock_result)
    {
        allocator_free(allocator, ctx);
        CloseHandle(handle);
        return NULL;
    }
    int fd = _open_osfhandle((intptr_t)handle, _O_RDWR);
    ctx->allocator = allocator;
    ctx->file = _fdopen(fd, "w+");
    return (LockFileContext*)ctx;
}

bool os_unlock_file(LockFileContext* context)
{
    LockFileContextWindows* ctx = (LockFileContextWindows*)context;
    if (context == NULL)
    {
        return false;
    }
    fflush(ctx->file);
    bool succeeded = UnlockFileEx(
        ctx->handle,
        0,
        MAXDWORD,
        MAXDWORD,
        &ctx->overlapped);
    fclose(ctx->file);
    allocator_free(ctx->allocator, context);
    return succeeded;
}

int os_ftruncate(FILE* f, long size)
{
    return _chsize(_fileno(f), size);
}

bool os_file_writable(const char* path)
{
    if (!os_file_exists(path))
    {
        return true;
    }
    FILE* try_file = os_fopen(path, "a");
    if (try_file == NULL)
    {
        return false;
    }
    else
    {
        fclose(try_file);
    }

    HANDLE h = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    CloseHandle(h);
    return true;
}

bool os_is_terminal_supports_color(void)
{
#define isatty _isatty
#define fileno _fileno

    static bool b_terminal_supports_color = false;
    static bool b_terminal_supports_color_checked = false;
    if (!b_terminal_supports_color_checked)
    {
        b_terminal_supports_color_checked = true;
        if (!isatty(fileno(stderr)))
        {
            b_terminal_supports_color = false;
            return false;
        }

        HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE)
        {
            b_terminal_supports_color = false;
            return false;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode))
        {
            b_terminal_supports_color = false;
            return false;
        }
        b_terminal_supports_color = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    }
    return b_terminal_supports_color;
#undef isatty
#undef fileno
}

void os_set_console_utf8(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}


#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <assert.h>
#include <userenv.h>

#pragma comment(lib, "Userenv.lib")

struct RegAPI
{
    long(WINAPI* RegOpenKeyExA)(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
    long(WINAPI* RegEnumKeyExA)(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime);
    long(WINAPI* RegCloseKey)(HKEY hKey);
    long(WINAPI* RegQueryValueExA)(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
};

static struct RegAPI* msvc_get_reg_api()
{
    static struct RegAPI RegAPI;
    if (RegAPI.RegOpenKeyExA == NULL)
    {
        Dylib* advapi32_dll = dylib_load("Advapi32.dll");
        if (advapi32_dll == NULL)
        {
            error("CToolchain_init_arch failed: can not found Advapi32.dll");
            exit(EXIT_FAILURE);
        }
        else
        {
            RegAPI.RegOpenKeyExA = dylib_get_symbol(advapi32_dll, "RegOpenKeyExA");
            RegAPI.RegEnumKeyExA = dylib_get_symbol(advapi32_dll, "RegEnumKeyExA");
            RegAPI.RegCloseKey = dylib_get_symbol(advapi32_dll, "RegCloseKey");
            RegAPI.RegQueryValueExA = dylib_get_symbol(advapi32_dll, "RegQueryValueExA");
        }
    }
    return &RegAPI;
}

static char const* msvc_find_sdk(char** latest_version, Allocator* allocator)
{
    HKEY h_key;
    const char* root_sub_key = "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
    char root_path[512];
    DWORD root_path_size = sizeof(root_path);
    DWORD index = 0;
    *latest_version = string_from_c_str(allocator, "");
    struct RegAPI* reg_api = msvc_get_reg_api();

    LONG result = reg_api->RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        root_sub_key,
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &h_key);
    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    while (1)
    {
        char version[32];
        DWORD name_length = sizeof(version);
        result = reg_api->RegEnumKeyExA(
            h_key,
            index,
            version,
            &name_length,
            NULL,
            NULL,
            NULL,
            NULL);

        if (result == ERROR_NO_MORE_ITEMS)
        {
            break;
        }
        else if (result != ERROR_SUCCESS)
        {
            reg_api->RegCloseKey(h_key);
            return NULL;
        }

        if (strchr(version, '.') != NULL &&
            strcmp(version, *latest_version) > 0)
        {
            array_resize(allocator, *latest_version, 0);
            string_concat_c_str(allocator, *latest_version, version);
        }
        index++;
    }
    reg_api->RegCloseKey(h_key);

    if (array_size(*latest_version) == 0)
    {
        return NULL;
    }

    result = reg_api->RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        root_sub_key,
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &h_key);
    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    result = reg_api->RegQueryValueExA(
        h_key,
        "KitsRoot10",
        NULL,
        NULL,
        (LPBYTE)root_path,
        &root_path_size);
    reg_api->RegCloseKey(h_key);

    if (result != ERROR_SUCCESS)
    {
        return NULL;
    }

    return string_from_c_str(allocator, root_path);
}

static char* msvc_find_installation_path(Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* pf86 = os_get_env(temp_allocator, "ProgramFiles(x86)");
    if (pf86 == NULL)
    {
        error("os_get_env: cannot get env: ProgramFiles(x86)");
        return NULL;
    }
    char const* vswhere_path = path_combine(temp_allocator, pf86, "Microsoft Visual Studio\\Installer\\vswhere.exe", NULL);
    if (!os_file_exists(vswhere_path))
    {
        error("vswhere.exe not found!");
        return NULL;
    }
    char const* cmd = string_from_print(temp_allocator, "\"%s\" -products * -latest -property installationPath", vswhere_path);
    FILE* f = os_popen(cmd, "rb");
    char* result = NULL;
    if (f)
    {
        while (true)
        {
            int ch = getc(f);
            if (ch == EOF)
            {
                break;
            }
            if (ch != '\r')
            {
                array_push(allocator, result, (char)ch);
            }
            else
            {
                array_push(allocator, result, 0);
                array_pop(result);
                break;
            }
        }
        os_pclose(f);
    }
    if (array_size(result) == 0)
    {
        error("msvc not found!");
        return NULL;
    }
    allocator_destroy(temp_allocator);
    return result;
}

static char const* msvc_find_latest_version(char const* root_path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* msvc_dir = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC", root_path);
    Directory* d = directory_open(msvc_dir, temp_allocator);
    if (d == NULL)
    {
        allocator_destroy(temp_allocator);
        return NULL;
    }
    char version[32] = "";
    while (true)
    {
        DirectoryEntry* entry = directory_read(d);
        if (entry == NULL)
        {
            directory_close(d);
            allocator_destroy(temp_allocator);
            break;
        }
        if (string_equal(entry->name, ".") || string_equal(entry->name, ".."))
        {
            continue;
        }
        if (entry->is_directory)
        {
            if (strchr(entry->name, '.') != NULL &&
                strcmp(entry->name, version) > 0)
            {
                strcpy(version, entry->name);
            }
        }
    }
    if (version[0] == 0)
    {
        return NULL;
    }
    return string_from_c_str(allocator, version);
}

static bool msvc_get_env_strings(
    Allocator* allocator,
    ArchitectureType type,
    char** out_include,
    char** out_lib,
    char** out_path)
{
    char const* arch = NULL;
    if (type == ARCH_X86)
    {
        arch = "x86";
    }
    else if (type == ARCH_X64)
    {
        arch = "x64";
    }
    else
    {
        error("Unsupported architecture!");
        exit(EXIT_FAILURE);
    }

    Allocator* temp_allocator = allocator_temp();
    char* sdk_version = NULL;
    char const* sdk_path = msvc_find_sdk(&sdk_version, temp_allocator);
    char const* vs_path = msvc_find_installation_path(temp_allocator);
    if (vs_path == NULL)
    {
        return false;
    }
    char const* msvc_path = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC\\", vs_path);
    char const* msvc_version = msvc_find_latest_version(vs_path, temp_allocator);
    char* new_include = NULL;
    {
        string_printf(allocator, new_include, "%sInclude\\%s\\ucrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\um;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\shared;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\winrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%sInclude\\%s\\cppwinrt;", sdk_path, sdk_version);
        string_printf(allocator, new_include, "%s%s\\include;", msvc_path, msvc_version);
    }
    char* new_lib = NULL;
    {
        string_printf(allocator, new_lib, "%sLib\\%s\\um\\%s;", sdk_path, sdk_version, arch);
        string_printf(allocator, new_lib, "%sLib\\%s\\ucrt\\%s;", sdk_path, sdk_version, arch);
        string_printf(allocator, new_lib, "%s%s\\lib\\%s;", msvc_path, msvc_version, arch);
    }
    char* new_path = NULL;
    {
        string_printf(allocator, new_path, "%s%s\\bin\\Host%s\\%s;", msvc_path, msvc_version, arch, arch);
        string_printf(allocator, new_path, "%sbin\\%s\\%s;", sdk_path, sdk_version, arch);
    }
    *out_include = new_include;
    *out_lib = new_lib;
    *out_path = new_path;
    return true;
}

static int msvc_gen_env_file(Node* cmd)
{
    ArchitectureType type = (ArchitectureType)(uintptr_t)cmd->ctx;
    Allocator* allocator = allocator_create_chained();
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    FILE* out_file = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto Error;
    }
    char const* strings[] = {
        new_include,
        new_lib,
        new_path,
    };
    out_file = os_fopen(cmd->outputs[0]->path, "wb");
    if (!out_file)
    {
        goto Error;
    }
    size_t n = fwrite(&type, sizeof(ArchitectureType), 1, out_file);
    if (n != 1)
    {
        goto Error;
    }
    for (size_t i = 0; i != static_array_size(strings); i++)
    {
        char const* p = strings[i];
        uint32_t num_bytes = array_bytes(p);
        n = fwrite(&num_bytes, sizeof(uint32_t), 1, out_file);
        if (n != 1)
        {
            goto Error;
        }
        n = fwrite(p, 1, num_bytes, out_file);
        if (n != num_bytes)
        {
            goto Error;
        }
    }
    fclose(out_file);
    out_file = NULL;
    int exit_code = EXIT_SUCCESS;
    goto Found;
Error:
    if (out_file)
    {
        fclose(out_file);
    }
    exit_code = EXIT_FAILURE;
Found:
    allocator_destroy(allocator);
    return exit_code;
}

static bool msvc_gen_env_file_check_dirty(Node* cmd)
{
    if (cmd_check_dirty(cmd))
    {
        return true;
    }
    ArchitectureType type = (ArchitectureType)(uintptr_t)cmd->ctx;
    Allocator* allocator = allocator_create_chained();
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    FILE* file = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto Dirty;
    }
    bool b_dirty = false;
    Node* output = cmd->outputs[0];
    file = os_fopen(output->path, "rb");
    ArchitectureType old_type;
    size_t n = fread(&old_type, sizeof(ArchitectureType), 1, file);
    if (n != 1 || old_type != type)
    {
        goto Dirty;
    }
    char const* strings[] = {
        new_include,
        new_lib,
        new_path,
    };
    for (size_t i = 0; i != static_array_size(strings); i++)
    {
        char const* p = strings[i];
        uint32_t num_bytes = 0;
        n = fread(&num_bytes, sizeof(uint32_t), 1, file);
        if (n != 1 || num_bytes != array_bytes(p))
        {
            goto Dirty;
        }
        char* old_str = allocator_malloc(allocator, num_bytes);
        n = fread(old_str, 1, num_bytes, file);
        if (n != num_bytes || memcmp(p, old_str, num_bytes) != 0)
        {
            goto Dirty;
        }
    }
    goto NotDirty;
Dirty:
    b_dirty = true;
NotDirty:
    if (file)
    {
        fclose(file);
    }
    allocator_destroy(allocator);
    return b_dirty;
}

static wchar_t* envs[ARCH_COUNT];

bool msvc_set_arch_env(ArchitectureType type)
{
    if (type == ARCH_UNSPECIFIED)
    {
        return true;
    }
    Allocator* allocator = allocator_create_tiny(512, 4096);
    char* new_include = NULL;
    char* new_lib = NULL;
    char* new_path = NULL;
    if (!msvc_get_env_strings(allocator, type, &new_include, &new_lib, &new_path))
    {
        goto NotFound;
    }

    char const* old_path = os_get_env(allocator, "Path");
    new_path = string_from_print(allocator, "%s;%s", new_path, old_path);
    os_set_env("INCLUDE", new_include);
    os_set_env("LIB", new_lib);
    os_set_env("Path", new_path);
    bool ret = true;
    goto Found;
NotFound:
    ret = false;
Found:
    assert(allocator);
    allocator_destroy(allocator);
    return ret;
}

static void msvc_make_env_block(ArchitectureType arch, wchar_t** out)
{
    wchar_t* default_env = os_get_default_env();
    if (msvc_set_arch_env(arch))
    {
        *out = os_get_env_block(allocator_c());
        SetEnvironmentStringsW(default_env);
    }
    else
    {
        *out = NULL;
    }
}

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    if (toolchain_type != TOOLCHAIN_TYPE_MSVC)
    {
        return NULL;
    }
    char const* path = NULL;
    if (arch == ARCH_X64)
    {
        path = "{out_dir}/envs/msvc_x64";
    }
    else if (arch == ARCH_X86)
    {
        path = "{out_dir}/envs/msvc_x86";
    }
    else
    {
        error("Not supported!");
        exit(EXIT_FAILURE);
    }
    Node* node = FILE(path);
    if (node->build_cmd == NULL)
    {
        Node* cmd = CALLBACK_CMD(msvc_gen_env_file, (void*)arch);
        cmd_add_output(cmd, node);
        node_set_check_dirty_fn(cmd, msvc_gen_env_file_check_dirty);
        if (envs[arch] == NULL)
        {
            msvc_make_env_block(arch, &envs[arch]);
        }
        node->env_block = envs[arch];
    }
    return node;
}

Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return msvc_get_env_node(toolchain_type, arch);
}

char const* msvc_find_std_module_source(bool b_compat)
{
    char const* filename = "std.ixx";
    if (b_compat)
    {
        filename = "std.compat.ixx";
    }

    Allocator* temp_allocator = allocator_temp();
    char const* vs_dir = msvc_find_installation_path(temp_allocator);
    if (!vs_dir)
    {
        return NULL;
    }
    char const* msvc_path = string_from_print(temp_allocator, "%s\\VC\\Tools\\MSVC\\", vs_dir);
    char const* msvc_version = msvc_find_latest_version(vs_dir, temp_allocator);
    if (!msvc_version)
    {
        return NULL;
    }
    char const* module_path = string_from_print(temp_allocator, "%s%s\\modules\\%s", msvc_path, msvc_version, filename);
    return module_path;
}

ToolchainType c_toolchain_select_toolchain_automatically()
{
    ToolchainType toolchain = get_toolchain_by_current_compiler();
    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
    bool b_no_msvc = false;
    bool b_no_llvm = false;
    bool b_no_gcc = false;
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        if (msvc_find_installation_path(temp_allocator))
        {
            return TOOLCHAIN_TYPE_MSVC;
        }
        else
        {
            b_no_msvc = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (system("clang --version > NUL 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_LLVM;
        }
        else
        {
            b_no_llvm = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        if (system("gcc -v > NUL 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_GCC;
        }
        else
        {
            b_no_gcc = true;
        }
    }
    if (!b_no_msvc && msvc_find_installation_path(temp_allocator))
    {
        return TOOLCHAIN_TYPE_MSVC;
    }
    if (!b_no_llvm && system("clang --version > NUL 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (!b_no_gcc && system("gcc -v > NUL 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    error("Cannot find the C compiler");
    exit(EXIT_FAILURE);
}

#include <assert.h>

void executor_force_kill_task(ExecutorSlot* slot)
{
    if (slot->task->b_thread)
    {
        if (slot->thread)
        {
            TerminateThread(slot->thread, 1);
            CloseHandle(slot->process);
            slot->thread = NULL;
        }
    }
    else
    {
        if (slot->process)
        {
            TerminateProcess(slot->process, 1);
            CloseHandle(slot->process);
            slot->process = NULL;
        }
        if (slot->read_stdout_ctx.read_pipe_handle)
        {
            CloseHandle(slot->read_stdout_ctx.read_pipe_handle);
            slot->read_stdout_ctx.read_pipe_handle = NULL;
        }
        if (slot->read_stderr_ctx.read_pipe_handle)
        {
            CloseHandle(slot->read_stderr_ctx.read_pipe_handle);
            slot->read_stderr_ctx.read_pipe_handle = NULL;
        }
    }
}

void executor_platform_init(Executor* executor)
{
    executor->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    assert(executor->iocp != NULL);
    wchar_t* env = os_get_default_env();
    executor_set_default_env(executor, env);
}

void executor_platform_destroy(Executor* executor)
{
    CloseHandle(executor->iocp);
}

static DWORD WINAPI executor_thread_wrapper(void* context)
{
    ExecutorSlot* slot = context;
    Task* task = slot->task;
    DWORD exit_code = task->thread_fn(task, task->ctx);
    PostQueuedCompletionStatus(slot->executor->iocp, 0, (ULONG_PTR)slot, &slot->overlapped);
    return exit_code;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    DWORD thread_id;
    slot->overlapped = (OVERLAPPED){0};
    slot->thread = CreateThread(NULL, 0, executor_thread_wrapper, slot, 0, &thread_id);
}

static char* executor_generate_unique_pipe_name(Allocator* allocator)
{
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char* guid = os_create_guid(stack_allocator, false);
    char* pipe_name = string_from_print(allocator, "\\\\.\\pipe\\%s", guid);
    return pipe_name;
}

static HANDLE executor_create_pipe_pair(ReadPipeContext* ctx)
{
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL};

    char* pipe_name = executor_generate_unique_pipe_name(allocator_temp());

    HANDLE read_pipe = CreateNamedPipeA(
        pipe_name,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE,
        1,
        OUTPUT_BUFFER_SIZE,
        OUTPUT_BUFFER_SIZE,
        0,
        NULL);

    assert(read_pipe != INVALID_HANDLE_VALUE);

    HANDLE write_pipe = CreateFileA(
        pipe_name,
        GENERIC_WRITE,
        0,
        &sa,
        OPEN_EXISTING,
        0,
        NULL);

    assert(write_pipe != INVALID_HANDLE_VALUE);

    ctx->read_pipe_handle = read_pipe;
    return write_pipe;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes);

static void executor_read_pipe(ReadPipeContext* ctx)
{
    DWORD bytes;
    ctx->overlapped = (OVERLAPPED){0};
    if (!ReadFile(ctx->read_pipe_handle, ctx->buffer, OUTPUT_BUFFER_SIZE, &bytes, &ctx->overlapped))
    {
        DWORD last_error = GetLastError();
        if (last_error == ERROR_BROKEN_PIPE)
        {
            CloseHandle(ctx->read_pipe_handle);
            ctx->read_pipe_handle = NULL;
        }
        else
        {
            assert(last_error == ERROR_IO_PENDING);
        }
    }
}

static HANDLE executor_init_read_pipe_context(ReadPipeContext* ctx, HANDLE iocp, ULONG_PTR key)
{
    if (ctx->write_buffer == NULL)
    {
        ctx->write_buffer = executor_default_write_buffer;
    }

    HANDLE write_pipe = executor_create_pipe_pair(ctx);

    if (!CreateIoCompletionPort(ctx->read_pipe_handle, iocp, key, 0))
    {
        assert(false && "CreateIoCompletionPort failed!\n");
    }

    executor_read_pipe(ctx);
    return write_pipe;
}

static HANDLE executor_create_nul_file()
{
    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    HANDLE const nul = CreateFileA(
        "NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &security_attributes,
        OPEN_EXISTING,
        0,
        NULL);
    if (nul == INVALID_HANDLE_VALUE)
    {
        assert(false && "couldn't open nul");
    }
    return nul;
}

static void executor_write_buffer(ReadPipeContext* ctx, char const* buffer, size_t num_bytes)
{
    ctx->write_buffer(ctx->write_buffer_ctx, buffer, num_bytes);
}

static char const* executor_create_process_error_message(DWORD error)
{
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND: return "executable not found";
    case ERROR_PATH_NOT_FOUND: return "executable path not found";
    case ERROR_BAD_EXE_FORMAT: return "bad executable format";
    case ERROR_ACCESS_DENIED: return "executable access denied";
    case ERROR_DIRECTORY: return "executable directory name is invalid";
    case ERROR_INVALID_NAME: return "executable name is invalid";
    case ERROR_INVALID_PARAMETER: return "invalid process parameter";
    default: return "CreateProcessW failed";
    }
}

static void executor_setup_env(Executor* executor, wchar_t* env_block)
{
    if (env_block != executor->current_env_block)
    {
        if (env_block == NULL)
        {
            SetEnvironmentStringsW(executor->default_env_block);
            executor->current_env_block = NULL;
        }
        else
        {
            SetEnvironmentStringsW(env_block);
            executor->current_env_block = env_block;
        }
    }
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    HANDLE stdout_write_pipe_end = executor_init_read_pipe_context(&slot->read_stdout_ctx, slot->executor->iocp, (ULONG_PTR)slot);
    HANDLE stderr_write_pipe_end = executor_init_read_pipe_context(&slot->read_stderr_ctx, slot->executor->iocp, (ULONG_PTR)slot);
    HANDLE const nul = executor_create_nul_file();
    STARTUPINFOW si = {
        .cb = sizeof(STARTUPINFOW),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = nul,
        .hStdOutput = stdout_write_pipe_end,
        .hStdError = stderr_write_pipe_end,
    };
    PROCESS_INFORMATION pi = {0};
    wchar_t* w_cmdline = utf8_to_wchars(allocator_temp(), slot->task->cmdline);
    executor_setup_env(slot->executor, slot->task->env_block);
    if (!CreateProcessW(NULL, w_cmdline, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
    {
        DWORD const error = GetLastError();
        char const* msg = string_from_print(allocator_temp(), "error: %s\n", executor_create_process_error_message(error));
        executor_write_buffer(&slot->read_stderr_ctx, msg, string_length(msg));
        slot->task->exit_code = EXIT_FAILURE;
    }
    else
    {
        CloseHandle(pi.hThread);
    }
    slot->process = pi.hProcess;
    CloseHandle(stdout_write_pipe_end);
    CloseHandle(stderr_write_pipe_end);
}

static void executor_update_process_pipe(ReadPipeContext* read_pipe_ctx)
{
    HANDLE read_pipe = read_pipe_ctx->read_pipe_handle;
    OVERLAPPED* overlapped = &read_pipe_ctx->overlapped;
    DWORD bytes;
    if (!GetOverlappedResult(read_pipe, overlapped, &bytes, TRUE))
    {
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            CloseHandle(read_pipe_ctx->read_pipe_handle);
            read_pipe_ctx->read_pipe_handle = NULL;
            return;
        }
    }
    if (bytes)
    {
        read_pipe_ctx->write_buffer(read_pipe_ctx->write_buffer_ctx, read_pipe_ctx->buffer, bytes);
    }
    executor_read_pipe(read_pipe_ctx);
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    if (slot->read_stderr_ctx.read_pipe_handle == NULL &&
        slot->read_stdout_ctx.read_pipe_handle == NULL)
    {
        return true;
    }
    return false;
}

static void executor_update_slot(ExecutorSlot* slot, OVERLAPPED* overlapped)
{
    if (slot->task->b_thread)
    {
        slot->b_finished = true;
        WaitForSingleObject(slot->thread, INFINITE);
        return;
    }
    if (overlapped == &slot->read_stderr_ctx.overlapped)
    {
        executor_update_process_pipe(&slot->read_stderr_ctx);
        if (slot->read_stderr_ctx.read_pipe_handle == NULL)
        {
            slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
        }
    }
    if (overlapped == &slot->read_stdout_ctx.overlapped)
    {
        executor_update_process_pipe(&slot->read_stdout_ctx);
        if (slot->read_stdout_ctx.read_pipe_handle == NULL)
        {
            slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
        }
    }
    if (executor_is_process_finished(slot))
    {
        slot->b_finished = true;
    }
}

static int executor_get_slot_exit_code(ExecutorSlot* slot)
{
    DWORD exit_code = slot->task->exit_code;
    if (slot->task->b_thread)
    {
        if (slot->thread != NULL)
        {
            GetExitCodeThread(slot->thread, &exit_code);
        }
    }
    else
    {
        if (slot->process)
        {
            GetExitCodeProcess(slot->process, &exit_code);
        }
    }
    return (int)exit_code;
}

static void executor_clear_slot(ExecutorSlot* slot)
{
    if (slot->task->b_thread)
    {
        if (slot->thread)
        {
            CloseHandle(slot->thread);
        }
    }
    else
    {
        if (slot->process)
        {
            CloseHandle(slot->process);
        }
    }
    slot->task = NULL;
}

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    slot->executor = executor;
    if (task->b_thread)
    {
        slot->thread = NULL;
    }
    else
    {
        slot->process = NULL;
    }
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        DWORD bytes_read;
        ULONG_PTR key;
        OVERLAPPED* overlapped;
        if (!GetQueuedCompletionStatus(executor->iocp, &bytes_read, &key, &overlapped, INFINITE))
        {
            assert(GetLastError() == ERROR_BROKEN_PIPE);
        }
        ExecutorSlot* slot = (ExecutorSlot*)key;
        executor_update_slot(slot, overlapped);
    }
    uint32_t slot_id = executor_find_finished_slot(executor);
    if (slot_id)
    {
        executor->num_running_tasks -= 1;
        ExecutorSlot* slot = executor_get_slot(executor, slot_id);
        Task* task = slot->task;
        task->exit_code = executor_get_slot_exit_code(slot);
        executor_clear_slot(slot);
        executor_flush(executor);
        return task;
    }
    return NULL;
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    task->env_block = env_block;
}

void executor_set_default_env(Executor* executor, wchar_t* env_block)
{
    executor->default_env_block = env_block;
}

typedef struct Graph Graph;
typedef struct Vcxproj Vcxproj;
typedef struct VcxprojFilter VcxprojFilter;
typedef struct StringStringHash StringStringHash;
typedef struct StringSet StringSet;
typedef struct StringPtrHash StringPtrHash;
typedef struct SlnFile SlnFile;

char const* vs_version = "vs2026";
char const* platform_toolset = "v145";

struct VcxprojFilter
{
    char const* path;
    char const* name;
    char const* guid;
};

struct SlnFile
{
    Allocator* allocator;
    char const* version;
    Vcxproj** projects;
    char const* configuration;
    char const* architecture;
    StringStringHash* hash_folder_to_parent_guid;
    StringStringHash* hash_folder_path_to_guid;
};

struct Vcxproj
{
    Allocator* allocator;
    char* name;
    char* path;
    char* guid;
    char const* config_name;
    char const* arch_name;
    char const* target_path;
    char* folder_path;
    CLanguageStandard c_std;
    CppLanguageStandard cxx_std;
    char const** sources;
    char const** headers;
    char const** defines;
    char const** includes;
    char const* debugger_working_dir;
    char const** debugger_args;
    Vcxproj** dependencies;
    StringSet* hash_set_files;
    StringSet* hash_set_defines;
    StringSet* hash_set_includes;
    StringPtrHash* hash_source_to_defines;
    StringPtrHash* hash_source_to_includes;
    bool b_exclude_in_build;
    bool b_executable;
};

static Vcxproj* vcxproj_create(
    Allocator* allocator,
    char* name,
    char* path,
    char* guid,
    char const* config_name,
    char const* arch_name,
    char* target_path,
    char* folder_path,
    char const* debugger_working_dir,
    char const** debugger_args)
{
    Vcxproj* p = allocator_calloc(allocator, 1, sizeof(Vcxproj));
    p->allocator = allocator;
    p->hash_set_files = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_files->allocator = allocator;
    p->hash_set_defines = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_defines->allocator = allocator;
    p->hash_set_includes = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_includes->allocator = allocator;
    p->hash_source_to_defines = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    p->hash_source_to_defines->allocator = allocator;
    p->hash_source_to_includes = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    p->hash_source_to_includes->allocator = allocator;
    p->name = name;
    p->path = path;
    p->guid = guid;
    p->config_name = config_name;
    p->arch_name = arch_name;
    p->target_path = target_path;
    p->folder_path = folder_path;
    p->debugger_working_dir = debugger_working_dir;
    p->debugger_args = debugger_args;
    if (string_equal(p->arch_name, "x86"))
    {
        p->arch_name = "Win32";
    }
    return p;
}

static void vcxproj_file_peek(FILE* file, char* buffer, uint64_t size)
{
    long o = ftell(file);
    size_t num_read = fread(buffer, 1, size, file);
    (void)num_read;
    if (fseek(file, o, SEEK_SET) != 0)
    {
        error("fseek error!");
    }
}
static void vcxproj_file_eat(FILE* file, uint64_t size)
{
    if (fseek(file, (long)size, SEEK_CUR) != 0)
    {
        error("fseek error!");
    }
}

static char* vcxproj_guid_from_existed_project(char const* path, Allocator* allocator)
{
    char* result = NULL;
    Allocator* arena_allocator = allocator_create_chained();
    FILE* cs = os_fopen(path, "rt");
    char tag[] = "<ProjectGuid>";
    uint64_t tag_len = sizeof(tag) - 1;
    bool found = false;
    for (;;)
    {
        char peek_buffer[sizeof(tag) - 1];
        vcxproj_file_peek(cs, peek_buffer, tag_len);
        if (peek_buffer[0] == EOF)
        {
            break;
        }
        found = true;
        for (uint64_t i = 0; i != tag_len; i++)
        {
            if (tag[i] != peek_buffer[i])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            vcxproj_file_eat(cs, tag_len + 1);
            break;
        }
        else
        {
            vcxproj_file_eat(cs, 1);
        }
    }

    if (!found)
    {
        goto Cleanup;
    }

    for (;;)
    {
        char ch;
        vcxproj_file_peek(cs, &ch, 1);
        if (ch != '}')
        {
            array_push(allocator, result, (char)ch);
            vcxproj_file_eat(cs, 1);
        }
        else
        {
            goto Success;
        }
    }
Cleanup:
    result = NULL;
Success:
    fclose(cs);
    allocator_destroy(arena_allocator);
    string_putc(allocator, result, '\0');
    return result;
}

static char* vcxproj_sln_guid_from_file(char const* path, Allocator* allocator)
{
    char* result = NULL;
    Allocator* arena_allocator = allocator_create_chained();
    FILE* fs = os_fopen(path, "rt");
    assert(fs);
    char tag[] = "SolutionGuid";
    uint64_t tag_len = sizeof(tag) - 1;
    bool found = false;
    for (;;)
    {
        char peek_buffer[sizeof(tag) - 1];
        vcxproj_file_peek(fs, peek_buffer, tag_len);
        if (peek_buffer[0] == EOF)
        {
            break;
        }
        found = true;
        for (uint64_t i = 0; i != tag_len; i++)
        {
            if (tag[i] != peek_buffer[i])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            vcxproj_file_eat(fs, tag_len + 1);
            break;
        }
        else
        {
            vcxproj_file_eat(fs, 1);
        }
    }
    if (!found)
    {
        goto Cleanup;
    }
    for (;;)
    {
        char ch;
        vcxproj_file_peek(fs, &ch, 1);
        vcxproj_file_eat(fs, 1);
        if (ch == '{')
        {
            break;
        }
    }
    for (;;)
    {
        char ch;
        vcxproj_file_peek(fs, &ch, 1);
        if (ch != '}')
        {
            array_push(allocator, result, (char)ch);
            vcxproj_file_eat(fs, 1);
        }
        else
        {
            goto Success;
        }
    }
Cleanup:
    result = NULL;
Success:
    fclose(fs);
    allocator_destroy(arena_allocator);
    string_putc(allocator, result, '\0');
    return result;
}

static char* vcxproj_find_or_create_guid(char const* path, Allocator* allocator)
{
    char* guid = NULL;
    if (os_file_exists(path))
    {
        guid = vcxproj_guid_from_existed_project(path, allocator);
    }
    if (!guid)
    {
        return os_create_guid(allocator, false);
    }
    return guid;
}

static char* sln_find_or_create_guid(char const* path, Allocator* allocator)
{
    char* guid = NULL;
    if (os_file_exists(path))
    {
        guid = vcxproj_sln_guid_from_file(path, allocator);
    }
    if (!guid)
    {
        return os_create_guid(allocator, false);
    }
    return guid;
}

static void vcxproj_add_include_dir(Vcxproj* project, char const* include_dir)
{
    if (hash_key_existed(project->hash_set_includes, include_dir))
    {
        return;
    }
    Allocator* temp_allocator = allocator_arena_from_alloca(4096);
    char const* full_path = os_full_path(include_dir, temp_allocator);
    char* dir = string_from_c_str(project->allocator, full_path);
    path_slash_to_backslash(dir);
    array_push(project->allocator, project->includes, dir);
    hash_insert(project->hash_set_includes, (char const*)dir);
}
static char const* vcxproj_find_header_for_source(char const* source, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* workspace = get_var("workspace");
    char const* src_stem = path_stem(source, temp_allocator);
    char const* parent = path_parent_path(source, temp_allocator);
    char const* full_parent_parent = NULL;
    char const* parent_parent = NULL;
    char const* header = NULL;
    if (parent[0] == 0)
    {
        goto NOT_FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.h", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.inl", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.hpp", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    parent_parent = path_parent_path(parent, temp_allocator);
    full_parent_parent = os_full_path(parent_parent, temp_allocator);
    if (!path_is_under_directory(full_parent_parent, workspace))
    {
        goto NOT_FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.h", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.inl", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.hpp", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
NOT_FOUND:
    allocator_destroy(temp_allocator);
    return NULL;
FOUND:
    header = string_from_c_str(allocator, header);
    allocator_destroy(temp_allocator);
    return header;
}

static char const* path_to_vs_path(char const* path)
{
    if (!path_is_absolute(path))
    {
        char const* workspace = get_var("workspace");
        path = path_combine(allocator_temp(), workspace, path, NULL);
    }
    else
    {
        path = string_from_c_str(allocator_temp(), path);
    }
    path_slash_to_backslash((char*)path);
    return path;
}

void vcxproj_add_header(Vcxproj* project, char const* header)
{
    if (hash_key_existed(project->hash_set_files, header))
    {
        return;
    }
    hash_insert(project->hash_set_files, header);
    array_push(project->allocator, project->headers, header);
}

void vcxproj_add_source(Vcxproj* project, char const* source)
{
    if (hash_key_existed(project->hash_set_files, source))
    {
        return;
    }
    hash_insert(project->hash_set_files, source);

    array_push(project->allocator, project->sources, source);

    char const* header = vcxproj_find_header_for_source(source, project->allocator);
    if (header)
    {
        vcxproj_add_header(project, header);
    }
}

void vcxproj_add_define_for_source(Vcxproj* project, char const* source, char const* define)
{
    if (hash_key_existed(project->hash_set_defines, define))
    {
        return;
    }
    bool b_existed;
    uint32_t index = hash_insert_check(project->hash_source_to_defines, source, &b_existed);
    StringSet* defines;
    if (!b_existed)
    {
        defines = allocator_calloc(project->allocator, 1, sizeof(StringSet));
        defines->allocator = project->allocator;
        hash_value(project->hash_source_to_defines, index) = defines;
    }
    else
    {
        defines = (StringSet*)hash_value(project->hash_source_to_defines, index);
    }
    char const* key = string_from_c_str(project->allocator, define);
    hash_insert(defines, key);
}

void vcxproj_add_include_dir_for_source(Vcxproj* project, char const* source, char const* include_dir)
{
    if (hash_key_existed(project->hash_set_includes, include_dir))
    {
        return;
    }
    bool b_existed;
    uint32_t index = hash_insert_check(project->hash_source_to_includes, source, &b_existed);
    StringSet* includes;
    if (!b_existed)
    {
        includes = allocator_calloc(project->allocator, 1, sizeof(StringSet));
        includes->allocator = project->allocator;
        hash_value(project->hash_source_to_includes, index) = includes;
    }
    else
    {
        includes = (StringSet*)hash_value(project->hash_source_to_includes, index);
    }
    hash_insert(includes, include_dir);
}

extern OptimizationType default_optimization_type;
extern ArchitectureType default_architecture_type;
extern CLanguageStandard default_c_std;
extern CppLanguageStandard default_cpp_std;

static Vcxproj* target_vcxproj_from_node(Node* node, Allocator* allocator)
{
    if (node->node_type != NODE_TYPE_CMD)
    {
        return NULL;
    }
    if (node->cmd_type == CMD_TYPE_EXECUTABLE && node->cmd_ext_type == C_CMD_COMPILE)
    {
        return NULL;
    }
    if (array_size(node->outputs) == 0)
    {
        return NULL;
    }
    Allocator* temp_allocator = allocator_create_chained();
    Node* output = node->outputs[0];
    char* target_path = output->path;
    char const* dir = path_parent_path(target_path, temp_allocator);
    char* sln_folder = dir[0] ? string_from_print(allocator, "targets\\%s", dir) : "targets";
    char const* out_dir = get_var("out_dir");
    char* project_name = path_filename(target_path, allocator);
    char* project_path = string_from_print(allocator, "%s\\ProjectFiles\\%s\\%s.vcxproj", out_dir, sln_folder, project_name);
    char const* cfg_name = get_optimization_string(default_optimization_type);
    char const* cwd = get_var("workspace");
    path_slash_to_backslash(project_path);
    Vcxproj* p = vcxproj_create(
        allocator,
        project_name,
        project_path,
        vcxproj_find_or_create_guid(project_path, allocator),
        cfg_name,
        get_arch_string(default_architecture_type),
        target_path,
        sln_folder,
        cwd, output->debugger_run_arguments);
    allocator_destroy(temp_allocator);
    p->b_executable = output->file_type == FILE_TYPE_EXE;
    return p;
}

static void sln_add_project(SlnFile* sln, Vcxproj* p)
{
    if (!sln->architecture)
    {
        sln->architecture = string_from_c_str(sln->allocator, p->arch_name);
    }
    if (!sln->configuration)
    {
        sln->configuration = string_from_c_str(sln->allocator, p->config_name);
    }
    if (!string_equal(sln->architecture, p->arch_name))
    {
        warn("Only one platform is supported at a time!");
        warn("    Old: %s", sln->configuration);
        warn("    New: %s", p->arch_name);
    }
    if (!string_equal(sln->configuration, p->config_name))
    {
        warn("Only one configuration is supported at a time!");
        warn("    Old: %s", sln->configuration);
        warn("    New: %s", p->config_name);
    }
    array_push(sln->allocator, sln->projects, p);
}

static void vcxproj_add_dependency(Vcxproj* p, Vcxproj* dep)
{
    Vcxproj* f = array_find(p->dependencies, array_pointer_compare, &dep);
    if (!f)
    {
        array_push(p->allocator, p->dependencies, dep);
    }
}

static void vcxproj_set_deps(Vcxproj* p, Node* cmd, Hash* hash_node_to_vcxproj)
{
    Allocator* temp_allocator = allocator_create_chained();
    Node** stack = NULL;
    Set set = {.allocator = temp_allocator};
    array_push(temp_allocator, stack, cmd);
    while (array_size(stack) != 0)
    {
        Node* node = array_pop(stack);
        bool b_existed;
        hash_insert_check(&set, (uintptr_t)node, &b_existed);
        if (b_existed)
        {
            continue;
        }
        Vcxproj* dependency = (Vcxproj*)(uintptr_t)hash_get(hash_node_to_vcxproj, (uintptr_t)node);
        if (dependency && (dependency != p))
        {
            vcxproj_add_dependency(p, dependency);
        }
        for (size_t i = 0; i != array_size(node->dependencies); i++)
        {
            Node* prev = node->dependencies[i];
            array_push(temp_allocator, stack, prev);
        }
    }
    allocator_destroy(temp_allocator);
}

static char const* vcxproj_sln_add_folder_path(StringStringHash* hash, StringStringHash* hash_folder_to_parent_guid, char const* path, Allocator* allocator)
{
    if (!path_has_relative_path(path))
    {
        return NULL;
    }
    char* parent = path_parent_path(path, allocator);
    char const* parent_guid = vcxproj_sln_add_folder_path(hash, hash_folder_to_parent_guid, parent, allocator);
    bool b_existed;
    uint32_t index = hash_insert_check(hash, path, &b_existed);
    char const* guid;
    if (!b_existed)
    {
        guid = os_create_guid(allocator, false);
        char const* key = string_from_c_str(allocator, path);
        hash_value(hash, index) = guid;
        hash_key(hash, index) = key;
    }
    else
    {
        guid = hash_value(hash, index);
    }
    if (parent_guid)
    {
        hash_put(hash_folder_to_parent_guid, guid, parent_guid);
    }
    return guid;
}

static void vcxproj_make_sln_folder_hash(Vcxproj** projects, StringStringHash* hash_folder_to_parent_guid, StringStringHash* hash_folder_path_to_guid, Allocator* allocator)
{
    for (uint64_t i = 0; i != array_size(projects); i++)
    {
        Vcxproj* p = projects[i];
        if (p->folder_path)
        {
            vcxproj_sln_add_folder_path(hash_folder_path_to_guid, hash_folder_to_parent_guid, p->folder_path, allocator);
        }
    }
}

static char* vcxproj_get_command(Vcxproj const* project, Allocator* allocator)
{
    return fmt_alloc(allocator, "cd $(SolutionDir) &amp;&amp; {self_name}");
}

static char* vcxproj_get_build_command(Vcxproj const* project, Allocator* allocator)
{
    char* cmd = vcxproj_get_command(project, allocator);
    if (project->target_path)
    {
        return string_from_print(allocator, "%s %s", cmd, project->target_path);
    }
    else
    {
        return string_from_print(allocator, "%s", cmd);
    }
}
static char const* vcxproj_get_clean_command(Vcxproj const* project, Allocator* allocator)
{
    char* clean_command = vcxproj_get_build_command(project, allocator);
    string_concat_c_str(allocator, clean_command, " -clean");
    return clean_command;
}
static char const* vcxproj_get_compile_command(Vcxproj const* project, Allocator* allocator)
{
    char* cmd = vcxproj_get_command(project, allocator);
    string_concat_c_str(allocator, cmd, " -compile \"$(SelectedFiles)\"");
    return cmd;
}

static char* vcxproj_concat_strings(char const** strings, Allocator* allocator)
{
    char* result = NULL;
    uint64_t num = array_size(strings);
    for (uint64_t i = 0; i != num; i++)
    {
        string_concat_c_str(allocator, result, strings[i]);
        if (i != num - 1)
        {
            array_push(allocator, result, ';');
        }
    }
    array_push(allocator, result, '\0');
    array_pop(result);
    return result;
}
static char* vcxproj_get_source_defines(Vcxproj const* project, char const* source, Allocator* allocator)
{
    StringSet* defines = (StringSet*)hash_get(project->hash_source_to_defines, source);
    char* result = NULL;
    if (defines)
    {
        for (uint64_t i = defines->begin; i != defines->end; i++)
        {
            if (hash_index_existed(defines, i))
            {
                char const* define = hash_key(defines, i);
                if (result)
                {
                    string_putc(allocator, result, ';');
                }
                string_concat_c_str(allocator, result, define);
            }
        }
    }
    return result;
}

static char* vcxproj_get_source_includes(Vcxproj const* project, char const* source, Allocator* allocator)
{
    StringSet* includes = (StringSet*)hash_get(project->hash_source_to_includes, source);
    if (!includes)
    {
        return NULL;
    }
    char* result = NULL;
    for (uint32_t i = includes->begin; i != includes->end; i = hash_next(includes, i))
    {
        char const* include = hash_key(includes, i);
        if (result)
        {
            string_putc(allocator, result, ';');
        }
        string_printf(allocator, result, "$(SolutionDir)%s", include);
    }
    return result;
}

char const* vcxproj_to_string(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    char const* out_dir = get_var("out_dir");

    char* xml = NULL;
    string_printf(allocator, xml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    string_printf(allocator, xml, "<Project DefaultTargets=\"Build\" ToolsVersion=\"16.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");
    // PropertyGroup
    string_printf(allocator, xml, "    <PropertyGroup>\n");
    string_printf(allocator, xml, "        <ProjectName>%s</ProjectName>\n", project->name);
    string_printf(allocator, xml, "        <ProjectGuid>{%s}</ProjectGuid>\n", project->guid);
    string_printf(allocator, xml, "        <PlatformToolset>%s</PlatformToolset>\n", platform_toolset);
    string_printf(allocator, xml, "        <ConfigurationType>Makefile</ConfigurationType>\n");
    string_printf(allocator, xml, "        <OutDir>$(SolutionDir)%s\\</OutDir>\n", out_dir);
    string_printf(allocator, xml, "        <NMakeBuildCommandLine>%s</NMakeBuildCommandLine>\n", vcxproj_get_build_command(project, arena_allocator));
    string_printf(allocator, xml, "        <NMakeCleanCommandLine>%s</NMakeCleanCommandLine>\n", vcxproj_get_clean_command(project, arena_allocator));
    if (project->includes)
    {
        char const* includes = vcxproj_concat_strings(project->includes, allocator);
        string_printf(allocator, xml, "        <NMakeIncludeSearchPath>%s</NMakeIncludeSearchPath>\n", includes);
    }
    string_printf(allocator, xml, "    </PropertyGroup>\n");
    if (project->sources && array_size(project->sources) > 0)
    {
        string_printf(allocator, xml, "    <ItemDefinitionGroup>\n");
        string_printf(allocator, xml, "        <NMakeCompile>\n");
        string_printf(allocator, xml, "            <NMakeCompileFileCommandLine>%s</NMakeCompileFileCommandLine>\n", vcxproj_get_compile_command(project, arena_allocator));
        string_printf(allocator, xml, "        </NMakeCompile>\n");
        string_printf(allocator, xml, "    </ItemDefinitionGroup>\n");
    }

    // Configuration Platform
    string_printf(allocator, xml, "    <ItemGroup>\n");
    string_printf(allocator, xml, "        <ProjectConfiguration Include=\"%s|%s\">\n", project->config_name, project->arch_name);
    string_printf(allocator, xml, "            <Configuration>%s</Configuration>\n", project->config_name);
    string_printf(allocator, xml, "            <Platform>%s</Platform>\n", project->arch_name);
    string_printf(allocator, xml, "        </ProjectConfiguration>\n");
    string_printf(allocator, xml, "    </ItemGroup>\n");
    // Import
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n");
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n");
    // Files
    string_printf(allocator, xml, "    <ItemGroup>\n");
    if (project->sources && array_size(project->sources) > 0)
    {
        for (uint64_t i = 0; i != array_size(project->sources); i++)
        {
            string_printf(allocator, xml, "        <ClCompile Include=\"%s\">\n", path_to_vs_path(project->sources[i]));
            if (string_ends_with(project->sources[i], ".cpp"))
            {
                string_printf(allocator, xml, "            <AdditionalOptions>/TP</AdditionalOptions>\n");
            }
            else
            {
                string_printf(allocator, xml, "            <AdditionalOptions>/TC</AdditionalOptions>\n");
            }
            // Add defines
            {
                char* defines = vcxproj_get_source_defines(project, project->sources[i], allocator);
                if (defines)
                {
                    string_printf(allocator, xml, "            <PreprocessorDefinitions>%s;%%(PreprocessorDefinitions)</PreprocessorDefinitions>\n", defines);
                    array_free(allocator, defines);
                }
            }
            // Add includes
            {
                char* includes = vcxproj_get_source_includes(project, project->sources[i], allocator);
                if (includes)
                {
                    string_printf(allocator, xml, "            <AdditionalIncludeDirectories>%s;%%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n", includes);
                    array_free(allocator, includes);
                }
            }
            string_printf(allocator, xml, "        </ClCompile>\n");
        }
    }
    if (project->headers)
    {
        for (uint64_t i = 0; i != array_size(project->headers); i++)
        {
            string_printf(allocator, xml, "        <ClInclude Include=\"%s\"/>\n", path_to_vs_path(project->headers[i]));
        }
    }
    string_printf(allocator, xml, "    </ItemGroup>\n");
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n");
    string_printf(allocator, xml, "</Project>");
    allocator_destroy(arena_allocator);
    return xml;
}

char* slnx_to_string(SlnFile* sln, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    StringPtrHash h = {.allocator = temp_allocator};
    for (size_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_tolower(p->guid);
        path_backslash_to_slash(p->folder_path);
        bool b_existed;
        uint32_t j = hash_insert_check(&h, p->folder_path, &b_existed);
        Vcxproj** list = NULL;
        if (b_existed)
        {
            list = hash_value(&h, j);
        }
        array_push(temp_allocator, list, p);
        hash_value(&h, j) = (void*)list;
    }
    char* out_str = NULL;
    string_printf(allocator, out_str, "<Solution>\n");
    string_printf(allocator, out_str, "  <Configurations>\n");
    string_printf(allocator, out_str, "    <BuildType Name=\"%s\" />\n", sln->configuration);
    string_printf(allocator, out_str, "    <Platform Name=\"%s\" />\n", sln->architecture);
    string_printf(allocator, out_str, "  </Configurations>\n");
    for (uint32_t i = h.begin; i != h.end; i = hash_next(&h, i))
    {
        Vcxproj** list = hash_value(&h, i);
        char const* folder = hash_key(&h, i);
        if (folder[0])
        {
            string_printf(allocator, out_str, "  <Folder Name=\"/%s/\">\n", folder);
            for (size_t j = 0; j != array_size(list); j++)
            {
                Vcxproj* p = list[j];
                path_backslash_to_slash(p->path);
                string_printf(allocator, out_str, "    <Project Path=\"%s\" Id=\"%s\">\n", p->path, p->guid);
                string_printf(allocator, out_str, "      <BuildType Project=\"%s\" />\n", p->config_name);
                string_printf(allocator, out_str, "      <Platform Project=\"%s\" />\n", p->arch_name);
                string_printf(allocator, out_str, "    </Project>\n");
            }
            string_printf(allocator, out_str, "  </Folder>\n");
        }
        else
        {
            for (size_t j = 0; j != array_size(list); j++)
            {
                Vcxproj* p = list[j];
                path_backslash_to_slash(p->path);
                string_printf(allocator, out_str, "  <Project Path=\"%s\" Id=\"%s\">\n", p->path, p->guid);
                string_printf(allocator, out_str, "    <BuildType Project=\"%s\" />\n", p->config_name);
                string_printf(allocator, out_str, "    <Platform Project=\"%s\" />\n", p->arch_name);
                string_printf(allocator, out_str, "  </Project>\n");
            }
        }
    }
    string_printf(allocator, out_str, "</Solution>\n");
    return out_str;
}

char* sln_to_string(SlnFile* sln, char const* old_guid, Allocator* allocator)
{
    vcxproj_make_sln_folder_hash(sln->projects, sln->hash_folder_to_parent_guid, sln->hash_folder_path_to_guid, sln->allocator);

    Allocator* arena_allocator = allocator_create_chained();
    char* out_str = NULL;

    // Header
    string_printf(allocator, out_str, "Microsoft Visual Studio Solution File, Format Version 12.00\r\n");
    string_printf(allocator, out_str, "# Visual Studio Version 17\r\n");
    string_printf(allocator, out_str, "VisualStudioVersion = 17.0.31912.275\r\n");
    string_printf(allocator, out_str, "MinimumVisualStudioVersion = 10.0.40219.1\r\n");

    // Project section
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_printf(
            allocator,
            out_str,
            "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"{%s}\"\r\n",
            p->name,
            p->path,
            p->guid);
#if 0
        if (p->dependencies && array_size(p->dependencies))
        {
            string_printf(allocator, out_str, "\tProjectSection(ProjectDependencies) = postProject\r\n");
            for (uint64_t j = 0; j != array_size(p->dependencies); j++)
            {
                Vcxproj const* dep = p->dependencies[j];
                string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", dep->guid, dep->guid);
            }
            string_printf(allocator, out_str, "\tEndProjectSection\r\n");
        }
#endif
        string_printf(allocator, out_str, "EndProject\r\n");
    }

    // Folders
    StringStringHash* h = sln->hash_folder_path_to_guid;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* folder_path = hash_key(h, i);
        char const* stem = path_stem(folder_path, arena_allocator);
        char const* folder_guid = hash_value(h, i);
        string_printf(allocator, out_str, "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"%s\", \"%s\", \"{%s}\"\r\n", stem, stem, folder_guid);
        string_printf(allocator, out_str, "EndProject\r\n");
    }

    string_printf(allocator, out_str, "Global\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n");
    string_printf(allocator, out_str, "\t\t%s|%s = %s|%s\r\n", sln->configuration, sln->architecture, sln->configuration, sln->architecture);
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n");
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_printf(allocator, out_str, "\t\t{%s}.%s|%s.ActiveCfg = %s|%s\r\n", p->guid, sln->configuration, sln->architecture, sln->configuration, sln->architecture);
        if (!p->b_exclude_in_build)
        {
            string_printf(allocator, out_str, "\t\t{%s}.%s|%s.Build.0 = %s|%s\r\n", p->guid, sln->configuration, sln->architecture, sln->configuration, sln->architecture);
        }
    }
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");
    string_printf(allocator, out_str, "\tGlobalSection(SolutionProperties) = preSolution\r\n");
    char const* guid = old_guid;
    string_printf(allocator, out_str, "\t\tHideSolutionNode = FALSE\r\n");
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(NestedProjects) = preSolution\r\n");
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        char const* folder_guid = hash_get(sln->hash_folder_path_to_guid, p->folder_path);
        if (folder_guid)
        {
            string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", p->guid, folder_guid);
        }
    }

    h = sln->hash_folder_to_parent_guid;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* folder = hash_key(h, i);
        char const* parent = hash_value(h, i);
        string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", folder, parent);
    }
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(ExtensibilityGlobals) = postSolution\r\n");
    string_printf(allocator, out_str, "\t\tSolutionGuid = {%s}\r\n", guid);
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");
    string_printf(allocator, out_str, "EndGlobal\r\n");
    allocator_destroy(arena_allocator);
    return out_str;
}

static Vcxproj* src_project(Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* out_dir = get_var("out_dir");
    char const* cwd = get_var("workspace");
    char* project_path = string_from_print(allocator, "%s\\ProjectFiles\\source.vcxproj", out_dir);
    char* guid = vcxproj_find_or_create_guid(project_path, temp_allocator);
    char const* cfg_name = get_optimization_string(default_optimization_type);
    Vcxproj* p = vcxproj_create(
        allocator,
        "source",
        project_path,
        guid,
        cfg_name,
        get_arch_string(default_architecture_type),
        NULL,
        "",
        NULL,
        NULL);
    Node** nodes = get_all_nodes();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->file_type != FILE_TYPE_OBJ)
        {
            continue;
        }
        CCompileCmd* cc = obj_get_compile_cmd(node);
        if (cc == NULL)
        {
            continue;
        }
        char const* src_path = cc->src->path;
        if (path_is_absolute(src_path) && !path_is_under_directory(src_path, cwd))
        {
            continue;
        }
        vcxproj_add_source(p, src_path);
        char** includes = obj_get_compile_includes(node, temp_allocator);
        for (size_t j = 0; j != array_size(includes); j++)
        {
            char const* inc = includes[j];
            vcxproj_add_include_dir_for_source(p, src_path, inc);
        }
    }
    return p;
}

static SlnFile* sln_from_graph(Allocator* allocator)
{
    Hash hash_node_to_vcxproj = {.allocator = allocator};
    SlnFile* sln = allocator_calloc(allocator, 1, sizeof(SlnFile));
    sln->allocator = allocator;
    sln->hash_folder_path_to_guid = allocator_calloc(allocator, 1, sizeof(StringStringHash));
    sln->hash_folder_path_to_guid->allocator = allocator;
    sln->hash_folder_to_parent_guid = allocator_calloc(allocator, 1, sizeof(StringStringHash));
    sln->hash_folder_to_parent_guid->allocator = allocator;
    Node** nodes = get_all_nodes();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Vcxproj* p = target_vcxproj_from_node(nodes[i], allocator);
        if (p)
        {
            sln_add_project(sln, p);
            uint64_t key = (uintptr_t)nodes[i];
            hash_put(&hash_node_to_vcxproj, key, (uintptr_t)p);
        }
    }
    Vcxproj* src_proj = src_project(allocator);
    sln_add_project(sln, src_proj);
    Hash* h = &hash_node_to_vcxproj;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Node* node = (Node*)(uintptr_t)hash_key(h, i);
        Vcxproj* p = (Vcxproj*)(uintptr_t)hash_value(h, i);
        vcxproj_set_deps(p, node, h);
    }
    return sln;
}

static char const** vcxproj_get_files(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    char const** items[] = {project->sources, project->headers, NULL};
    char const** result = NULL;
    for (uint64_t j = 0; items[j]; j++)
    {
        char const** files = items[j];
        for (uint64_t i = 0; i != array_size(files); i++)
        {
            array_push(allocator, result, files[i]);
        }
    }
    allocator_destroy(arena_allocator);
    return result;
}

static int vcxproj_find_paths_common(char const** paths)
{
    int common_index = 0;
    uint64_t num_paths = array_size(paths);
    while (true)
    {
        uint64_t i = num_paths;
        char c = paths[0][common_index];
        do
        {
            if (paths[i - 1][common_index] != c)
            {
                break;
            }
            i--;
        } while (i && c);
        if (i != 0 || c == 0)
        {
            break;
        }
        ++common_index;
    }
    while (common_index != 0 && paths[0][common_index - 1] != '\\')
    {
        common_index--;
    }
    return common_index;
}

static VcxprojFilter* vcxproj_add_parent_filters(StringPtrHash* filters, char const* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    path = string_from_c_str(temp_allocator, path);
    VcxprojFilter* first = NULL;
    for (;;)
    {
        char* parent = path_parent_path(path, temp_allocator);
        if (parent[0] == 0)
        {
            break;
        }
        array_free(temp_allocator, path);
        path_slash_to_backslash(parent);
        path = parent;
        bool b_existed;
        uint32_t i = hash_insert_check(filters, path, &b_existed);
        VcxprojFilter* filter;
        if (!b_existed)
        {
            filter = allocator_malloc(allocator, sizeof(VcxprojFilter));
            filter->guid = os_create_guid(allocator, true);
            filter->path = string_from_c_str(allocator, path);
            filter->name = string_from_c_str(allocator, path);
            hash_value(filters, i) = filter;
            hash_key(filters, i) = filter->path;
        }
        else
        {
            filter = (VcxprojFilter*)hash_value(filters, i);
        }
        if (!first)
        {
            first = filter;
        }
    }
    allocator_destroy(temp_allocator);
    return first;
}

static VcxprojFilter** vcxproj_find_filters(Vcxproj const* project, StringPtrHash* hash_path_to_filter, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    StringPtrHash filters = {.allocator = temp_allocator};
    char const** files = vcxproj_get_files(project, allocator);

    VcxprojFilter** result = NULL;
    if (array_size(files) == 0)
    {
        allocator_destroy(temp_allocator);
        return result;
    }
    for (uint64_t i = 0; i != array_size(files); i++)
    {
        char const* p = files[i];
        VcxprojFilter* f = vcxproj_add_parent_filters(&filters, p, allocator);
        char const* key = files[i];
        hash_put(hash_path_to_filter, key, (void*)f);
    }
    for (uint32_t i = filters.begin; i != filters.end; i = hash_next(&filters, i))
    {
        VcxprojFilter* filter = (VcxprojFilter*)hash_value(&filters, i);
        array_push(allocator, result, filter);
    }
    allocator_destroy(temp_allocator);
    return result;
}

char* vcxproj_filters_to_string(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    StringPtrHash hash_path_to_filter = {.allocator = arena_allocator};
    VcxprojFilter** filters = vcxproj_find_filters(project, &hash_path_to_filter, arena_allocator);
    char* out = NULL;
    string_printf(allocator, out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    string_printf(allocator, out, "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    for (uint64_t i = 0; i != array_size(filters); i++)
    {
        VcxprojFilter* f = filters[i];
        string_printf(allocator, out, "        <Filter Include=\"%s\" />\n", f->name);
    }
    string_printf(allocator, out, "    </ItemGroup>\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    if (project->sources)
    {
        for (uint64_t i = 0; i != array_size(project->sources); i++)
        {
            char const* file = project->sources[i];
            VcxprojFilter* filter = (VcxprojFilter*)hash_get(&hash_path_to_filter, file);
            if (!filter)
            {
                continue;
            }
            string_printf(allocator, out, "        <ClCompile Include=\"%s\">\n", path_to_vs_path(file));
            string_printf(allocator, out, "            <Filter>%s</Filter>\n", filter->name);
            string_printf(allocator, out, "        </ClCompile>\n");
        }
    }
    string_printf(allocator, out, "    </ItemGroup>\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    if (project->headers)
    {
        for (uint64_t i = 0; i != array_size(project->headers); i++)
        {
            char const* file = project->headers[i];
            VcxprojFilter* filter = (VcxprojFilter*)hash_get(&hash_path_to_filter, file);
            if (!filter)
            {
                continue;
            }
            string_printf(allocator, out, "        <ClInclude Include=\"%s\">\n", path_to_vs_path(file));
            string_printf(allocator, out, "            <Filter>%s</Filter>\n", filter->name);
            string_printf(allocator, out, "        </ClInclude>\n");
        }
    }
    string_printf(allocator, out, "    </ItemGroup>\n");
    string_printf(allocator, out, "</Project>\n");

    allocator_destroy(arena_allocator);
    return out;
}

char* vcxproj_user_file_to_string(Vcxproj const* project, Allocator* allocator)
{
    char* content = NULL;
    string_printf(allocator, content, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
    string_printf(allocator, content, "<Project ToolsVersion=\"Current\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n");
    string_printf(allocator, content, "    <PropertyGroup>\r\n");
    string_printf(allocator, content, "        <LocalDebuggerWorkingDirectory>%s</LocalDebuggerWorkingDirectory>\r\n", project->debugger_working_dir);
    string_printf(allocator, content, "        <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>\r\n");
    string_printf(allocator, content, "        <LocalDebuggerCommand>$(SolutionDir)%s</LocalDebuggerCommand>\r\n", project->target_path);
    if (project->debugger_args)
    {
        string_printf(allocator, content, "        <LocalDebuggerCommandArguments>");
        for (size_t i = 0; i != array_size(project->debugger_args); i++)
        {
            char const* arg = project->debugger_args[i];
            if (i != 0)
            {
                string_printf(allocator, content, " ");
            }
            string_printf(allocator, content, "%s", arg);
        }
        string_printf(allocator, content, "</LocalDebuggerCommandArguments>\r\n");
    }
    string_printf(allocator, content, "    </PropertyGroup>\r\n");
    string_printf(allocator, content, "</Project>");
    return content;
}

bool b_generate_vs_projects = false;

void set_generate_vs_projects_enabled(bool enabled)
{
    b_generate_vs_projects = enabled;
}

static int gen_vs_projects_fn(Node* cmd)
{
    char const* sln_path = cmd->outputs[0]->path;
    Allocator* allocator = allocator_create_chained();
    SlnFile* sln = sln_from_graph(allocator);
    for (size_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        char const* new_content = vcxproj_to_string(p, allocator);
        char const* old_content = os_read_all(allocator, p->path);
        if (!old_content || !string_equal(old_content, new_content))
        {
            os_ensure_dir_existed(p->path);
            os_write_all(p->path, new_content, array_size(new_content));
        }
        array_free(allocator, old_content);
        array_free(allocator, new_content);
        if (p->sources)
        {
            char const* filters_path = string_from_print(allocator, "%s.filters", p->path);
            char* filters = vcxproj_filters_to_string(p, allocator);
            os_ensure_dir_existed(filters_path);
            os_write_all(filters_path, filters, array_size(filters));
        }
        if (p->b_executable)
        {
            char const* vcxproj_user = vcxproj_user_file_to_string(p, allocator);
            char const* user_file_path = string_from_print(allocator, "%s.user", p->path);
            char const* old_user_setting = os_read_all(allocator, user_file_path);
            if (!old_user_setting || !string_equal(vcxproj_user, old_user_setting))
            {
                os_ensure_dir_existed(p->path);
                os_write_all(user_file_path, vcxproj_user, array_size(vcxproj_user));
            }
            array_free(allocator, old_user_setting);
            array_free(allocator, vcxproj_user);
        }
    }
    char const* sln_content;
    if (string_equal(vs_version, "vs2022"))
    {
        sln_content = sln_to_string(sln, sln_find_or_create_guid(sln_path, allocator), allocator);
    }
    else if (string_equal(vs_version, "vs2026"))
    {
        sln_content = slnx_to_string(sln, allocator);
    }
    else
    {
        goto Failed;
    }
    os_ensure_dir_existed(sln_path);
    os_write_all(sln_path, sln_content, array_size(sln_content));
    allocator_destroy(allocator);
    return EXIT_SUCCESS;
Failed:
    allocator_destroy(allocator);
    return EXIT_FAILURE;
}

static bool gen_vs_projects_check_dirty(Node* cmd)
{
    return true;
}

void set_vs_project_version(char const* version)
{
    vs_version = version;
    if (string_equal(version, "vs2026"))
    {
        platform_toolset = "v145";
    }
    else if (string_equal(version, "vs2022"))
    {
        platform_toolset = "v143";
    }
}

ENTRY(gen_vs_projects)
{
    Node* cmd = CALLBACK_CMD(gen_vs_projects_fn, NULL);
    cmd->b_default_excluded = !b_generate_vs_projects;
    node_set_alias(cmd, "vsproj");
    node_set_check_dirty_fn(cmd, gen_vs_projects_check_dirty);
    Allocator* allocator = allocator_temp();
    char const* cwd = get_var("workspace");
    char const* project_name = path_stem(cwd, allocator);
    char* sln_path = string_from_print(allocator, "%s.slnx", project_name);
    Node* sln = get_or_add_file(sln_path);
    sln->b_default_excluded = !b_generate_vs_projects;
    cmd_add_output(cmd, sln);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", sln));
}
#elif CURRENT_PLATFORM == PLATFORM_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void* allocator_virtual_alloc(void* base_address, size_t size)
{
    void* ptr = mmap(base_address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void allocator_virtual_free(void* base_address, size_t size)
{
    if (munmap(base_address, size) == -1)
    {
        perror("munmap failed");
        exit(EXIT_FAILURE);
    }
}

#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>

typedef struct Directory
{
    Allocator* allocator;
    DIR* dir;
    DirectoryEntry current_entry;
    char* path;
} Directory;

Directory* directory_open(const char* path, Allocator* allocator)
{
    DIR* d = opendir(path);
    if (d)
    {
        Directory* directory = allocator_malloc(allocator, sizeof(Directory));
        directory->allocator = allocator;
        directory->dir = d;
        directory->current_entry.name = string_from_c_str(allocator, "");
        directory->path = string_from_c_str(allocator, path);
        return directory;
    }
    else
    {
        return NULL;
    }
}

DirectoryEntry* directory_read(Directory* dir)
{
    struct dirent* entry = readdir(dir->dir);
    if (entry != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            dir->current_entry.is_directory = true;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            char const* full_path = path_combine(allocator_temp(), dir->path, entry->d_name, NULL);
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                dir->current_entry.is_directory = S_ISDIR(st.st_mode);
            }
            else
            {
                dir->current_entry.is_directory = false;
            }
        }
        else
        {
            dir->current_entry.is_directory = false;
        }
        array_resize(dir->allocator, dir->current_entry.name, 0);
        string_printf(dir->allocator, dir->current_entry.name, "%s", entry->d_name);
        return &dir->current_entry;
    }
    else
    {
        return NULL;
    }
}

void directory_close(Directory* dir)
{
    array_free(dir->allocator, dir->current_entry.name);
    array_free(dir->allocator, dir->path);
    closedir(dir->dir);
    allocator_free(dir->allocator, dir);
}

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

uint64_t os_get_mtime(char const* path)
{
    struct stat file_stat;

    if (stat(path, &file_stat) == -1)
    {
        return 0;
    }
    return file_stat.st_mtime;
}

bool os_file_exists(char const* path)
{
    return access(path, F_OK) == 0;
}

char* os_get_cwd(Allocator* allocator)
{
    char* p = getcwd(NULL, 0);
    char* cwd = string_from_c_str(allocator, p);
    free(p);
    return cwd;
}

bool os_set_cwd(char const* path)
{
    return chdir(path) == 0;
}

bool os_mkdir(char const* path)
{
    if (mkdir(path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            return false;
        }
    }
    return true;
}

bool os_remove_file(char const* path)
{
    return remove(path) == 0;
}

bool os_rename(char const* old_path, char const* new_path)
{
    return rename(old_path, new_path) == 0;
}

bool os_copy_file(char const* src, char const* dst)
{
    if (fork() == 0)
    {
        execlp("cp", "cp", src, dst, NULL);
        exit(0);
    }
    wait(NULL);
    return true;
}

char* get_absolute_path(char const* path, Allocator* allocator)
{
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char* cwd = os_get_cwd(stack_allocator);
    if (!path_is_absolute(path))
    {
        return path_combine(allocator, cwd, path, NULL);
    }
    return string_from_c_str(allocator, path);
}

uint64_t os_get_file_size(char const* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return st.st_size;
    }
    return UINT64_MAX;
}

char* os_get_env(Allocator* allocator, const char* name)
{
    char* e = getenv(name);
    return string_from_c_str(allocator, e);
}

void os_set_env(char const* name, char const* env)
{
    setenv(name, env, 1);
}

void os_reset_env(void)
{
}

FILE* os_fopen(char const* path, char const* mode)
{
    return fopen(path, mode);
}

FILE* os_popen(char const* cmd, char const* mode)
{
    return popen(cmd, mode);
}

int os_pclose(FILE* file)
{
    return pclose(file);
}

int os_get_cpu_count(void)
{
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count == -1)
    {
        perror("sysconf failed");
        return 1;
    }
    return cpu_count;
}

Process* os_start_process(char const* cmd)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        Allocator* stack_allocator = allocator_arena_from_alloca(65535);
        char* cmd_copied = string_from_c_str(stack_allocator, cmd);
        char const* p = cmd_copied;
        char** argv = NULL;
        while (true)
        {
            char* arg = NULL;
            p = utilities_split_cmd(stack_allocator, p, &arg);
            if (array_size(arg) == 0)
            {
                array_push(stack_allocator, argv, NULL);
                break;
            }
            array_push(stack_allocator, argv, arg);
        }
        execvp(argv[0], argv);
        error("execv failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        return (Process*)(uintptr_t)pid;
    }
}

int os_wait_process(Process* p)
{
    pid_t pid = (uintptr_t)p;
    int status;
    pid_t wait_pid = waitpid(pid, &status, 0);
    if (wait_pid == -1)
    {
        error("waitpid failed");
        exit(EXIT_FAILURE);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

void os_forget_process(Process* process)
{
}

typedef struct LockFileContextLinux
{
    FILE* file;
    int fd;
    Allocator* allocator;
} LockFileContextLinux;

LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared)
{
    os_ensure_dir_existed(path);
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        perror("open failed");
        return NULL;
    }

    if (flock(fd, b_shared ? LOCK_SH : LOCK_EX) == -1)
    {
        perror("flock failed");
        close(fd);
        return NULL;
    }

    LockFileContextLinux* ctx = allocator_calloc(allocator, 1, sizeof(LockFileContextLinux));
    ctx->fd = fd;
    ctx->allocator = allocator;
    ctx->file = fdopen(fd, "w+");
    if (!ctx->file)
    {
        perror("fdopen failed");
        flock(fd, LOCK_UN);
        close(fd);
        allocator_free(allocator, ctx);
        return NULL;
    }

    return (LockFileContext*)ctx;
}

bool os_unlock_file(LockFileContext* context)
{
    LockFileContextLinux* ctx = (LockFileContextLinux*)context;
    if (!ctx)
    {
        return false;
    }

    fflush(ctx->file);
    bool succeeded = (flock(ctx->fd, LOCK_UN) == 0);
    fclose(ctx->file);
    allocator_free(ctx->allocator, context);
    return succeeded;
}

int os_ftruncate(FILE* f, long size)
{
    return ftruncate(fileno(f), size);
}

bool os_file_writable(char const* path)
{
    if (!os_file_exists(path))
    {
        return true;
    }
    FILE* try_file = os_fopen(path, "a");
    if (try_file == NULL)
    {
        return false;
    }
    else
    {
        fclose(try_file);
        return true;
    }
}

bool os_is_terminal_supports_color(void)
{
    static bool b_terminal_supports_color = false;
    static bool b_terminal_supports_color_checked = false;
    if (!b_terminal_supports_color_checked)
    {
        b_terminal_supports_color_checked = true;
        if (!isatty(fileno(stderr)))
        {
            b_terminal_supports_color = false;
            return false;
        }
        const char* term = getenv("TERM");
        b_terminal_supports_color = term != NULL && strcmp(term, "dumb") != 0;
    }
    return b_terminal_supports_color;
}

void os_set_console_utf8(void)
{
}



#include <dlfcn.h>
#include <unistd.h>

typedef struct Dylib
{
    char const* name;
    void* handle;
} Dylib;

Dylib* dylib_load(char const* name)
{
    void* handle = dlopen(name, RTLD_LAZY);
    if (!handle)
    {
        return NULL;
    }
    Allocator* ca = allocator_c();
    if (name == NULL)
    {
        name = os_get_current_exe_path(ca);
    }
    else
    {
        name = string_from_c_str(ca, name);
    }
    Dylib* lib = allocator_malloc(ca, sizeof(Dylib));
    lib->handle = handle;
    lib->name = name;
    return lib;
}

void dylib_unload(Dylib* lib)
{
    Allocator* ca = allocator_c();
    array_free(allocator_c(), lib->name);
    dlclose(lib->handle);
    allocator_free(ca, lib);
}

void* dylib_get_symbol(Dylib* lib, char const* name)
{
    return dlsym(lib->handle, name);
}

void* dylib_get_image_base(Dylib* lib)
{
    return lib->handle;
}

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char const* os_get_cmdline(void)
{
    FILE* file = fopen("/proc/self/cmdline", "r");
    if (!file)
    {
        perror("fopen");
        return NULL;
    }
    static char cmdline[65536];
    size_t bytesRead = fread(cmdline, 1, sizeof(cmdline) - 1, file);
    fclose(file);
    for (size_t i = 0; i < bytesRead; i++)
    {
        if (cmdline[i] == '\0')
        {
            cmdline[i] = ' ';
        }
    }
    return cmdline;
}

char* get_absolute_path(char const* path, Allocator* allocator);

char* os_get_current_exe_path(Allocator* allocator)
{
    Dl_info info;
    dladdr(os_get_current_exe_path, &info);
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char const* path = get_absolute_path(info.dli_fname, stack_allocator);
    return path_lexically_normal(path, allocator);
}

uint64_t os_get_rand_uint64()
{
    uint64_t r;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        perror("open /dev/urandom");
        return 0;
    }
    if (read(fd, &r, sizeof(r)) != sizeof(r))
    {
        perror("read");
        close(fd);
        return 0;
    }
    close(fd);
    return r;
}

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

void executor_platform_init(Executor* executor)
{
    executor->epoll_fd = epoll_create1(0);
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        slot->ctx_thread = EPOLL_CTX_THREAD;
        slot->ctx_stdout = EPOLL_CTX_STDOUT;
        slot->ctx_stderr = EPOLL_CTX_STDERR;
        slot->epoll_fd = executor->epoll_fd;
    }
}

void executor_platform_destroy(Executor* executor)
{
    close(executor->epoll_fd);
}

void executor_read_pipe(ReadPipeContext* ctx)
{
    char buffer[OUTPUT_BUFFER_SIZE];
    ssize_t bytes = read(ctx->read_pipe, buffer, OUTPUT_BUFFER_SIZE);
    if (bytes)
    {
        ctx->write_buffer(ctx->write_buffer_ctx, buffer, bytes);
    }
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    if (slot->read_stdout_ctx.read_pipe == -1 && slot->read_stderr_ctx.read_pipe == -1)
    {
        return true;
    }
    return false;
}

void executor_update_process(ExecutorSlot* slot, struct epoll_event* e, ReadPipeContext* ctx)
{
    if (e->events & EPOLLIN)
    {
        executor_read_pipe(ctx);
    }
    if (e->events & (EPOLLHUP | EPOLLERR))
    {
        if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, ctx->read_pipe, NULL) == -1)
        {
            perror("epoll_ctl DEL");
        }
        close(ctx->read_pipe);
        ctx->read_pipe = -1;
    }
    if (executor_is_process_finished(slot))
    {
        int status;
        pid_t pid = waitpid(slot->pid, &status, 0);
        if (pid == -1)
        {
            assert(false && "waitpid failed");
        }
        slot->task->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
        slot->b_finished = true;
        slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
        slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
    }
}

void executor_update_thread(ExecutorSlot* slot, struct epoll_event* e)
{
    if (e->events & EPOLLIN)
    {
        uint64_t signal_value;
        ssize_t n = read(slot->event_fd, &signal_value, sizeof(signal_value));
        if (n == -1)
        {
            perror("read event_fd");
        }
        void* return_value;
        pthread_join(slot->thread_id, &return_value);
        if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, slot->event_fd, NULL) == -1)
        {
            perror("epoll_ctl DEL");
        }
        close(slot->event_fd);
        slot->event_fd = -1;
        slot->b_finished = true;
    }
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        struct epoll_event e;
        int n = epoll_wait(executor->epoll_fd, &e, 1, -1);
        if (n != 1)
        {
            return NULL;
        }
        EpollContextType* ctx_type = (EpollContextType*)e.data.ptr;
        if (*ctx_type == EPOLL_CTX_STDERR)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stderr));
            executor_update_process(slot, &e, &slot->read_stderr_ctx);
        }
        else if (*ctx_type == EPOLL_CTX_STDOUT)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stdout));
            if (slot->task->b_thread)
            {
                executor_update_thread(slot, &e);
            }
            else
            {
                executor_update_process(slot, &e, &slot->read_stdout_ctx);
            }
        }
    }
    uint32_t slot_id = executor_find_finished_slot(executor);
    if (slot_id)
    {
        executor->num_running_tasks -= 1;
        ExecutorSlot* slot = executor_get_slot(executor, slot_id);
        Task* task = slot->task;
        slot->task = NULL;
        executor_flush(executor);
        return task;
    }
    return NULL;
}

void executor_default_write_buffer(void* ctx, char const* buffer, size_t num_bytes);

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (task->b_thread)
    {
        slot->event_fd = -1;
    }
    else
    {
        slot->pid = 0;
    }
}

static void* executor_callback_thread_warpper(void* ctx)
{
    ExecutorSlot* slot = ctx;
    Task* task = slot->task;
    task->exit_code = EXIT_FAILURE;
    task->exit_code = task->thread_fn(task, task->ctx);
    uint64_t signal_value = 1;
    ssize_t written = write(slot->event_fd, &signal_value, sizeof(signal_value));
    if (written == -1)
    {
        perror("write event_fd");
    }
    return NULL;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    ExecutorSlot* s = (ExecutorSlot*)slot;
    s->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    assert(s->event_fd != -1);
    struct epoll_event e = {.events = EPOLLIN, .data = {.ptr = &s->ctx_thread}};
    if (epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, s->event_fd, &e) == -1)
    {
        assert(false);
    }
    pthread_create(&s->thread_id, NULL, executor_callback_thread_warpper, s);
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    int pipe_stdout_fds[2];
    int pipe_stderr_fds[2];
    if (pipe(pipe_stdout_fds) == -1 || pipe(pipe_stderr_fds) == -1)
    {
        printf("pipe failed\n");
        exit(EXIT_FAILURE);
    }
    fcntl(pipe_stdout_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_stderr_fds[0], F_SETFL, O_NONBLOCK);

    slot->read_stdout_ctx.read_pipe = pipe_stdout_fds[0];
    slot->read_stderr_ctx.read_pipe = pipe_stderr_fds[0];
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = {.ptr = &slot->ctx_stdout},
    };
    epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, slot->read_stdout_ctx.read_pipe, &ev);
    ev.data = (epoll_data_t){.ptr = &slot->ctx_stderr};
    epoll_ctl(slot->epoll_fd, EPOLL_CTL_ADD, slot->read_stderr_ctx.read_pipe, &ev);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        close(pipe_stdout_fds[0]);
        close(pipe_stderr_fds[0]);

        dup2(pipe_stdout_fds[1], STDOUT_FILENO);
        dup2(pipe_stderr_fds[1], STDERR_FILENO);

        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);

        execl("/bin/sh", "sh", "-c", slot->task->cmdline, NULL);
        fprintf(stderr, "exec error: %s\n", slot->task->cmdline);
        perror("execv failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);
        slot->pid = pid;
    }
}

void executor_force_kill_task_process(ExecutorSlot* slot)
{
    assert(slot->task);
    if (slot->read_stdout_ctx.read_pipe != -1)
    {
        close(slot->read_stdout_ctx.read_pipe);
        slot->read_stdout_ctx.read_pipe = -1;
    }
    if (slot->read_stderr_ctx.read_pipe != -1)
    {
        close(slot->read_stderr_ctx.read_pipe);
        slot->read_stderr_ctx.read_pipe = -1;
    }
    if (slot->pid)
    {
        kill(slot->pid, SIGKILL);
        waitpid(slot->pid, NULL, 0);
    }
}

void executor_force_kill_task_thread(ExecutorSlot* slot)
{
    assert(slot->task);
    if (slot->event_fd != -1)
    {
        int result = epoll_ctl(slot->epoll_fd, EPOLL_CTL_DEL, slot->event_fd, NULL);
        if (result == -1)
        {
            if (errno != EBADF && errno != ENOENT)
            {
                perror("epoll_ctl DEL");
            }
        }
        close(slot->event_fd);
        slot->event_fd = -1;
    }
}

void executor_force_kill_task(ExecutorSlot* slot)
{
    if (slot->task)
    {
        if (slot->task->b_thread)
        {
            executor_force_kill_task_thread(slot);
        }
        else
        {
            executor_force_kill_task_process(slot);
        }
    }
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    assert(false && "Not implemented on Linux.");
}
#elif CURRENT_PLATFORM == PLATFORM_MACOS
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void* allocator_virtual_alloc(void* base_address, size_t size)
{
    void* ptr = mmap(base_address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void allocator_virtual_free(void* base_address, size_t size)
{
    if (munmap(base_address, size) == -1)
    {
        perror("munmap failed");
        exit(EXIT_FAILURE);
    }
}

#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>

typedef struct Directory
{
    Allocator* allocator;
    DIR* dir;
    DirectoryEntry current_entry;
    char* path;
} Directory;

Directory* directory_open(const char* path, Allocator* allocator)
{
    DIR* d = opendir(path);
    if (d)
    {
        Directory* directory = allocator_malloc(allocator, sizeof(Directory));
        directory->allocator = allocator;
        directory->dir = d;
        directory->current_entry.name = string_from_c_str(allocator, "");
        directory->path = string_from_c_str(allocator, path);
        return directory;
    }
    else
    {
        return NULL;
    }
}

DirectoryEntry* directory_read(Directory* dir)
{
    struct dirent* entry = readdir(dir->dir);
    if (entry != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            dir->current_entry.is_directory = true;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            char const* full_path = path_combine(allocator_temp(), dir->path, entry->d_name, NULL);
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                dir->current_entry.is_directory = S_ISDIR(st.st_mode);
            }
            else
            {
                dir->current_entry.is_directory = false;
            }
        }
        else
        {
            dir->current_entry.is_directory = false;
        }
        array_resize(dir->allocator, dir->current_entry.name, 0);
        string_printf(dir->allocator, dir->current_entry.name, "%s", entry->d_name);
        return &dir->current_entry;
    }
    else
    {
        return NULL;
    }
}

void directory_close(Directory* dir)
{
    array_free(dir->allocator, dir->current_entry.name);
    array_free(dir->allocator, dir->path);
    closedir(dir->dir);
    allocator_free(dir->allocator, dir);
}

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

uint64_t os_get_mtime(char const* path)
{
    struct stat file_stat;

    if (stat(path, &file_stat) == -1)
    {
        return 0;
    }
    return file_stat.st_mtime;
}

bool os_file_exists(char const* path)
{
    return access(path, F_OK) == 0;
}

char* os_get_cwd(Allocator* allocator)
{
    char* p = getcwd(NULL, 0);
    char* cwd = string_from_c_str(allocator, p);
    free(p);
    return cwd;
}

bool os_set_cwd(char const* path)
{
    return chdir(path) == 0;
}

bool os_mkdir(char const* path)
{
    if (mkdir(path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            return false;
        }
    }
    return true;
}

bool os_remove_file(char const* path)
{
    return remove(path) == 0;
}

bool os_rename(char const* old_path, char const* new_path)
{
    return rename(old_path, new_path) == 0;
}

bool os_copy_file(char const* src, char const* dst)
{
    if (fork() == 0)
    {
        execlp("cp", "cp", src, dst, NULL);
        exit(0);
    }
    wait(NULL);
    return true;
}

char* get_absolute_path(char const* path, Allocator* allocator)
{
    Allocator* stack_allocator = allocator_arena_from_alloca(4096);
    char* cwd = os_get_cwd(stack_allocator);
    if (!path_is_absolute(path))
    {
        return path_combine(allocator, cwd, path, NULL);
    }
    return string_from_c_str(allocator, path);
}

uint64_t os_get_file_size(char const* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return st.st_size;
    }
    return UINT64_MAX;
}

char* os_get_env(Allocator* allocator, const char* name)
{
    char* e = getenv(name);
    return string_from_c_str(allocator, e);
}

void os_set_env(char const* name, char const* env)
{
    setenv(name, env, 1);
}

void os_reset_env(void)
{
}

FILE* os_fopen(char const* path, char const* mode)
{
    return fopen(path, mode);
}

FILE* os_popen(char const* cmd, char const* mode)
{
    return popen(cmd, mode);
}

int os_pclose(FILE* file)
{
    return pclose(file);
}

int os_get_cpu_count(void)
{
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count == -1)
    {
        perror("sysconf failed");
        return 1;
    }
    return cpu_count;
}

Process* os_start_process(char const* cmd)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        Allocator* stack_allocator = allocator_arena_from_alloca(65535);
        char* cmd_copied = string_from_c_str(stack_allocator, cmd);
        char const* p = cmd_copied;
        char** argv = NULL;
        while (true)
        {
            char* arg = NULL;
            p = utilities_split_cmd(stack_allocator, p, &arg);
            if (array_size(arg) == 0)
            {
                array_push(stack_allocator, argv, NULL);
                break;
            }
            array_push(stack_allocator, argv, arg);
        }
        execvp(argv[0], argv);
        error("execv failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        return (Process*)(uintptr_t)pid;
    }
}

int os_wait_process(Process* p)
{
    pid_t pid = (uintptr_t)p;
    int status;
    pid_t wait_pid = waitpid(pid, &status, 0);
    if (wait_pid == -1)
    {
        error("waitpid failed");
        exit(EXIT_FAILURE);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

void os_forget_process(Process* process)
{
}

typedef struct LockFileContextLinux
{
    FILE* file;
    int fd;
    Allocator* allocator;
} LockFileContextLinux;

LockFileContext* os_lock_file(char const* path, Allocator* allocator, bool b_shared)
{
    os_ensure_dir_existed(path);
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        perror("open failed");
        return NULL;
    }

    if (flock(fd, b_shared ? LOCK_SH : LOCK_EX) == -1)
    {
        perror("flock failed");
        close(fd);
        return NULL;
    }

    LockFileContextLinux* ctx = allocator_calloc(allocator, 1, sizeof(LockFileContextLinux));
    ctx->fd = fd;
    ctx->allocator = allocator;
    ctx->file = fdopen(fd, "w+");
    if (!ctx->file)
    {
        perror("fdopen failed");
        flock(fd, LOCK_UN);
        close(fd);
        allocator_free(allocator, ctx);
        return NULL;
    }

    return (LockFileContext*)ctx;
}

bool os_unlock_file(LockFileContext* context)
{
    LockFileContextLinux* ctx = (LockFileContextLinux*)context;
    if (!ctx)
    {
        return false;
    }

    fflush(ctx->file);
    bool succeeded = (flock(ctx->fd, LOCK_UN) == 0);
    fclose(ctx->file);
    allocator_free(ctx->allocator, context);
    return succeeded;
}

int os_ftruncate(FILE* f, long size)
{
    return ftruncate(fileno(f), size);
}

bool os_file_writable(char const* path)
{
    if (!os_file_exists(path))
    {
        return true;
    }
    FILE* try_file = os_fopen(path, "a");
    if (try_file == NULL)
    {
        return false;
    }
    else
    {
        fclose(try_file);
        return true;
    }
}

bool os_is_terminal_supports_color(void)
{
    static bool b_terminal_supports_color = false;
    static bool b_terminal_supports_color_checked = false;
    if (!b_terminal_supports_color_checked)
    {
        b_terminal_supports_color_checked = true;
        if (!isatty(fileno(stderr)))
        {
            b_terminal_supports_color = false;
            return false;
        }
        const char* term = getenv("TERM");
        b_terminal_supports_color = term != NULL && strcmp(term, "dumb") != 0;
    }
    return b_terminal_supports_color;
}

void os_set_console_utf8(void)
{
}




#include <dlfcn.h>

typedef struct Dylib
{
    char const* name;
    void* handle;
} Dylib;

Dylib* dylib_load(char const* name)
{
    void* handle = dlopen(name, RTLD_LAZY);
    if (!handle)
    {
        return NULL;
    }
    Allocator* ca = allocator_c();
    if (name == NULL)
    {
        name = os_get_current_exe_path(ca);
    }
    else
    {
        name = string_from_c_str(ca, name);
    }
    Dylib* lib = allocator_malloc(ca, sizeof(Dylib));
    lib->handle = handle;
    lib->name = name;
    return lib;
}

void dylib_unload(Dylib* lib)
{
    Allocator* ca = allocator_c();
    array_free(allocator_c(), lib->name);
    dlclose(lib->handle);
    allocator_free(ca, lib);
}

void* dylib_get_symbol(Dylib* lib, char const* name)
{
    return dlsym(lib->handle, name);
}

void* dylib_get_image_base(Dylib* lib)
{
    return lib->handle;
}

#include <crt_externs.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char const* os_get_cmdline(void)
{
    static char cmdline[65536] = {0};

    if (cmdline[0] != '\0')
    {
        return cmdline;
    }

    int argc = *_NSGetArgc();
    char** argv = *_NSGetArgv();

    size_t offset = 0;
    for (int i = 0; i < argc; i++)
    {
        size_t len = strlen(argv[i]);
        if (offset + len + 1 >= sizeof(cmdline))
        {
            break;
        }
        memcpy(cmdline + offset, argv[i], len);
        offset += len;
        cmdline[offset++] = ' ';
    }

    if (offset > 0)
    {
        cmdline[offset - 1] = '\0';
    }

    return cmdline;
}

char* get_absolute_path(char const* path, Allocator* allocator);

char* os_get_current_exe_path(Allocator* allocator)
{
    char pathbuf[PATH_MAX];
    uint32_t size = sizeof(pathbuf);

    if (_NSGetExecutablePath(pathbuf, &size) == 0)
    {
        Allocator* stack_allocator = allocator_arena_from_alloca(4096);
        char const* path = get_absolute_path(pathbuf, stack_allocator);
        return path_lexically_normal(path, allocator);
    }

    return NULL;
}

uint64_t os_get_rand_uint64()
{
    uint64_t r;
    arc4random_buf(&r, sizeof(r));
    return r;
}

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static void kq_update_event(int kq, int fd, int filter, int flags, void* udata)
{
    struct kevent sev;
    EV_SET(&sev, fd, filter, flags, 0, 0, udata);
    if (kevent(kq, &sev, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent register");
    }
}

void executor_platform_init(Executor* executor)
{
    executor->kq_fd = kqueue();
    assert(executor->kq_fd != -1);
    for (size_t i = 0; i != executor->num_slots; i++)
    {
        ExecutorSlot* slot = &executor->slots[i];
        slot->ctx_thread = KQ_CTX_THREAD;
        slot->ctx_stdout = KQ_CTX_STDOUT;
        slot->ctx_stderr = KQ_CTX_STDERR;
        slot->kq_fd = executor->kq_fd;
    }
}

void executor_platform_set_slot(Executor* executor, uint32_t slot_id, Task* task)
{
    ExecutorSlot* slot = executor_get_slot(executor, slot_id);
    if (task->b_thread)
    {
        slot->thread_done_pipe[0] = -1;
        slot->thread_done_pipe[1] = -1;
    }
    else
    {
        slot->pid = 0;
    }
}

void executor_set_task_env_block(Task* task, wchar_t* env_block)
{
    assert(false && "Not implemented on macOS.");
}

void executor_platform_destroy(Executor* executor)
{
    close(executor->kq_fd);
}

void executor_read_pipe(ReadPipeContext* ctx)
{
    char buffer[OUTPUT_BUFFER_SIZE];
    ssize_t bytes = read(ctx->read_pipe, buffer, OUTPUT_BUFFER_SIZE);
    if (bytes > 0)
    {
        ctx->write_buffer(ctx->write_buffer_ctx, buffer, bytes);
    }
}

static bool executor_is_process_finished(ExecutorSlot* slot)
{
    return (slot->read_stdout_ctx.read_pipe == -1 && slot->read_stderr_ctx.read_pipe == -1);
}

void executor_update_process(ExecutorSlot* slot, struct kevent* e, ReadPipeContext* ctx)
{
    executor_read_pipe(ctx);

    if (e->flags & EV_EOF)
    {
        kq_update_event(slot->kq_fd, ctx->read_pipe, EVFILT_READ, EV_DELETE, NULL);
        close(ctx->read_pipe);
        ctx->read_pipe = -1;
    }

    if (executor_is_process_finished(slot))
    {
        int status;
        pid_t pid = waitpid(slot->pid, &status, 0);
        if (pid != -1)
        {
            slot->task->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
            slot->b_finished = true;
            slot->read_stderr_ctx.write_buffer(slot->read_stderr_ctx.write_buffer_ctx, NULL, 0);
            slot->read_stdout_ctx.write_buffer(slot->read_stdout_ctx.write_buffer_ctx, NULL, 0);
        }
    }
}

void executor_update_thread(ExecutorSlot* slot, struct kevent* e)
{
    char dummy;
    read(slot->thread_done_pipe[0], &dummy, 1);

    void* return_value;
    pthread_join(slot->thread_id, &return_value);

    kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_DELETE, NULL);
    close(slot->thread_done_pipe[0]);
    close(slot->thread_done_pipe[1]);
    slot->thread_done_pipe[0] = -1;
    slot->thread_done_pipe[1] = -1;

    slot->b_finished = true;
}

Task* executor_update(Executor* executor)
{
    if (executor->num_running_tasks > 0)
    {
        struct kevent e;
        int n = kevent(executor->kq_fd, NULL, 0, &e, 1, NULL);
        if (n <= 0) return NULL;

        KqueueContextType* ctx_type = (KqueueContextType*)e.udata;
        if (*ctx_type == KQ_CTX_STDERR)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stderr));
            executor_update_process(slot, &e, &slot->read_stderr_ctx);
        }
        else if (*ctx_type == KQ_CTX_STDOUT)
        {
            ExecutorSlot* slot = (ExecutorSlot*)((char*)ctx_type - offsetof(ExecutorSlot, ctx_stdout));
            if (slot->task->b_thread)
            {
                executor_update_thread(slot, &e);
            }
            else
            {
                executor_update_process(slot, &e, &slot->read_stdout_ctx);
            }
        }
    }

    uint32_t slot_id = executor_find_finished_slot(executor);
    if (slot_id)
    {
        executor->num_running_tasks -= 1;
        ExecutorSlot* slot = executor_get_slot(executor, slot_id);
        Task* task = slot->task;
        slot->task = NULL;
        executor_flush(executor);
        return task;
    }
    return NULL;
}

static void* executor_callback_thread_wrapper(void* ctx)
{
    ExecutorSlot* slot = ctx;
    Task* task = slot->task;
    task->exit_code = EXIT_FAILURE;
    task->exit_code = task->thread_fn(task, task->ctx);

    char signal_value = 1;
    ssize_t s = write(slot->thread_done_pipe[1], &signal_value, 1);
    (void)s;
    return NULL;
}

void executor_execute_slot_thread(ExecutorSlot* slot)
{
    if (pipe(slot->thread_done_pipe) == -1)
    {
        perror("pipe failed");
        return;
    }
    fcntl(slot->thread_done_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(slot->thread_done_pipe[1], F_SETFL, O_NONBLOCK);

    kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_ADD, &slot->ctx_thread);

    int result = pthread_create(&slot->thread_id, NULL, executor_callback_thread_wrapper, slot);
    assert(result == 0);
}

void executor_execute_slot_process(ExecutorSlot* slot)
{
    int pipe_stdout_fds[2];
    int pipe_stderr_fds[2];
    if (pipe(pipe_stdout_fds) == -1 || pipe(pipe_stderr_fds) == -1)
    {
        fprintf(stderr, "pipe failed\n");
        exit(EXIT_FAILURE);
    }
    fcntl(pipe_stdout_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_stderr_fds[0], F_SETFL, O_NONBLOCK);

    slot->read_stdout_ctx.read_pipe = pipe_stdout_fds[0];
    slot->read_stderr_ctx.read_pipe = pipe_stderr_fds[0];

    kq_update_event(slot->kq_fd, slot->read_stdout_ctx.read_pipe, EVFILT_READ, EV_ADD, &slot->ctx_stdout);
    kq_update_event(slot->kq_fd, slot->read_stderr_ctx.read_pipe, EVFILT_READ, EV_ADD, &slot->ctx_stderr);

    pid_t pid = fork();
    if (pid == -1) exit(EXIT_FAILURE);
    if (pid == 0)
    {
        close(pipe_stdout_fds[0]);
        close(pipe_stderr_fds[0]);
        dup2(pipe_stdout_fds[1], STDOUT_FILENO);
        dup2(pipe_stderr_fds[1], STDERR_FILENO);
        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);

        execl("/bin/sh", "sh", "-c", slot->task->cmdline, NULL);
        exit(EXIT_FAILURE);
    }
    else
    {
        close(pipe_stdout_fds[1]);
        close(pipe_stderr_fds[1]);
        slot->pid = pid;
    }
}

void executor_force_kill_task_process(ExecutorSlot* slot)
{
    if (slot->read_stdout_ctx.read_pipe != -1)
    {
        kq_update_event(slot->kq_fd, slot->read_stdout_ctx.read_pipe, EVFILT_READ, EV_DELETE, NULL);
        close(slot->read_stdout_ctx.read_pipe);
        slot->read_stdout_ctx.read_pipe = -1;
    }
    if (slot->read_stderr_ctx.read_pipe != -1)
    {
        kq_update_event(slot->kq_fd, slot->read_stderr_ctx.read_pipe, EVFILT_READ, EV_DELETE, NULL);
        close(slot->read_stderr_ctx.read_pipe);
        slot->read_stderr_ctx.read_pipe = -1;
    }
    if (slot->pid)
    {
        kill(slot->pid, SIGKILL);
        waitpid(slot->pid, NULL, 0);
    }
}

void executor_force_kill_task_thread(ExecutorSlot* slot)
{
    if (slot->thread_done_pipe[0] != -1)
    {
        kq_update_event(slot->kq_fd, slot->thread_done_pipe[0], EVFILT_READ, EV_DELETE, NULL);
        close(slot->thread_done_pipe[0]);
        close(slot->thread_done_pipe[1]);
        slot->thread_done_pipe[0] = -1;
        slot->thread_done_pipe[1] = -1;
    }
}

void executor_force_kill_task(ExecutorSlot* slot)
{
    if (slot->task)
    {
        if (slot->task->b_thread)
        {
            executor_force_kill_task_thread(slot);
        }
        else
        {
            executor_force_kill_task_process(slot);
        }
    }
}
#else
#error "unknown platform"
#endif

#if CURRENT_PLATFORM == PLATFORM_LINUX || CURRENT_PLATFORM == PLATFORM_MACOS

#include <assert.h>
#include <stdlib.h>

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return NULL;
}

char const* msvc_find_std_module_source(bool b_compat)
{
    return NULL;
}

Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return NULL;
}

ToolchainType c_toolchain_select_toolchain_automatically()
{
    ToolchainType toolchain = get_toolchain_by_current_compiler();
    bool b_no_llvm = false;
    bool b_no_gcc = false;
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (system("clang --version > /dev/null 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_LLVM;
        }
        else
        {
            b_no_llvm = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        if (system("gcc -v > /dev/null 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_GCC;
        }
        else
        {
            b_no_gcc = true;
        }
    }
    if (!b_no_llvm && system("clang --version > /dev/null 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (!b_no_gcc && system("gcc -v > /dev/null 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    error("Cannot find the C compiler");
    exit(EXIT_FAILURE);
}
#endif

// Generated by bin2c, don't edit!
// clang-format off
#include <stddef.h>
#include <stdint.h>

size_t bin2c_build_script_tpl_c_size = 2229;
char const bin2c_build_script_tpl_c[] =
    "I2luY2x1ZGUgImN1cC5oIg0KDQovLyBzZXRfYWZ0ZXJfcHJlcGFyZV9jYWxsYmFjayhzZXR1cF9kZWZh"
    "dWx0X2ZsYWdzKTsNCi8vIFRoZSBgc2V0dXBfZGVmYXVsdF9mbGFnc2AgZnVuY3Rpb24gd2lsbCBiZSBj"
    "YWxsZWQgYWZ0ZXIgdGhlIHByZXBhcmUgcGhhc2UsIGF0IHdoaWNoIHBvaW50DQovLyB0aGUgZGVwZW5k"
    "ZW5jaWVzIG9mIGFsbCB0YXJnZXRzIGhhdmUgYmVlbiBkZXRlcm1pbmVkIGFuZCBubyBuZXcgdGFyZ2V0"
    "cyB3aWxsIGJlIGdlbmVyYXRlZC4NCnZvaWQgc2V0dXBfZGVmYXVsdF9mbGFncyh2b2lkKQ0Kew0KICAg"
    "IE5vZGUqKiBub2RlcyA9IGdldF9hbGxfbm9kZXMoKTsNCiAgICBzaXplX3QgbnVtX25vZGVzID0gYXJy"
    "YXlfc2l6ZShub2Rlcyk7DQogICAgdWludDMyX3QgY29tcGlsZV9jbWRfdHlwZSA9IG5vZGVfbWFrZV9j"
    "bWRfdHlwZShDTURfVFlQRV9FWEVDVVRBQkxFLCBDX0NNRF9DT01QSUxFKTsNCiAgICB1aW50MzJfdCBs"
    "aW5rX2NtZF90eXBlID0gbm9kZV9tYWtlX2NtZF90eXBlKENNRF9UWVBFX0VYRUNVVEFCTEUsIENfQ01E"
    "X0xJTkspOw0KICAgIGZvciAoc2l6ZV90IGkgPSAwOyBpICE9IG51bV9ub2RlczsgaSsrKQ0KICAgIHsN"
    "CiAgICAgICAgTm9kZSogbm9kZSA9IG5vZGVzW2ldOw0KICAgICAgICBpZiAobm9kZS0+dHlwZSA9PSBj"
    "b21waWxlX2NtZF90eXBlKQ0KICAgICAgICB7DQogICAgICAgICAgICBDQ29tcGlsZUNtZCogY21kID0g"
    "KENDb21waWxlQ21kKilub2RlOw0KICAgICAgICAgICAgLy8gY19jb21waWxlX2NtZF9hZGRfaW5jbHVk"
    "ZV9kaXJlY3Rvcnkobm9kZSwgInNyYyIpOw0KICAgICAgICAgICAgLy8gY19jb21waWxlX2NtZF9zZXRf"
    "Y3BwX3N0ZChub2RlLCBDUFBfTEFOR1VBR0VfU1RBTkRBUkRfMjApOw0KICAgICAgICAgICAgLy8gY19j"
    "b21waWxlX2NtZF9zZXRfY19zdGQobm9kZSwgQ19MQU5HVUFHRV9TVEFOREFSRF8yMyk7DQogICAgICAg"
    "ICAgICBpZiAoY21kLT50b29sY2hhaW4gPT0gVE9PTENIQUlOX1RZUEVfTExWTSkNCiAgICAgICAgICAg"
    "IHsNCiAgICAgICAgICAgICAgICAvLyBjX2NvbXBpbGVfY21kX2FkZF9mbGFnKG5vZGUsICItV2FsbCIp"
    "Ow0KICAgICAgICAgICAgICAgIC8vIGNfY29tcGlsZV9jbWRfYWRkX2ZsYWcobm9kZSwgIi1XZXh0cmEi"
    "KTsNCiAgICAgICAgICAgIH0NCiAgICAgICAgICAgIC8vIGlmIChjbWQtPnRvb2xjaGFpbiAhPSBUT09M"
    "Q0hBSU5fVFlQRV9NU1ZDKQ0KICAgICAgICAgICAgLy8gew0KICAgICAgICAgICAgLy8gICAgIGNfY29t"
    "cGlsZV9jbWRfYWRkX2ZsYWcobm9kZSwgIi1mbXMtZXh0ZW5zaW9ucyIpOw0KICAgICAgICAgICAgLy8g"
    "ICAgIGNfY29tcGlsZV9jbWRfYWRkX2ZsYWcobm9kZSwgIi1Xbm8tZGVwcmVjYXRlZC1kZWNsYXJhdGlv"
    "bnMiKTsNCiAgICAgICAgICAgIC8vICAgICBjX2NvbXBpbGVfY21kX2FkZF9mbGFnKG5vZGUsICItV25v"
    "LW1pY3Jvc29mdC1hbm9uLXRhZyIpOw0KICAgICAgICAgICAgLy8gfQ0KICAgICAgICB9DQogICAgICAg"
    "IGlmIChub2RlLT50eXBlID09IGxpbmtfY21kX3R5cGUpDQogICAgICAgIHsNCiAgICAgICAgICAgIC8v"
    "IExpbmtDbWQqIGNtZCA9IChMaW5rQ21kKilub2RlOw0KICAgICAgICAgICAgLy8gbGlua19jbWRfYWRk"
    "X2ZsYWcobm9kZSwgImFueSBzdHJpbmcgYXBwZW5kIHRvIGxpbmsgY21kIik7DQogICAgICAgIH0NCiAg"
    "ICB9DQp9DQoNCmludCBtYWluKGludCBhcmdjLCBjaGFyKiogYXJndikNCnsNCiAgICAvLyBTZXQgdG8g"
    "YHRydWVgIHRvIGdlbmVyYXRlIFZTQ29kZSBsYXVuY2guanNvbiBhbmQgdGFza3MuanNvbi4NCiAgICAv"
    "LyBFdmVuIGlmIGl0IGlzIG5vdCBzZXQgdG8gYHRydWVgLCBpZiB0aGUgdGFyZ2V0IGB2c2NfbGF1bmNo"
    "YChhbGlhcyBvZiBgLnZzY29kZS9sYXVuY2guanNvbmApIGlzIHNwZWNpZmllZA0KICAgIC8vIG9uIHRo"
    "ZSBjb21tYW5kIGxpbmUsIGAudnNjb2RlL2xhdW5jaC5qc29uYCB3aWxsIHN0aWxsIGJlIGdlbmVyYXRl"
    "ZC4NCiAgICBzZXRfZ2VuZXJhdGVfdnNjb2RlX2ZpbGVzX2VuYWJsZWQoZmFsc2UpOw0KDQogICAgaWYg"
    "KGdldF9kZWZhdWx0X29wdGltaXphdGlvbigpICE9IE9QVElNSVpBVElPTl9UWVBFX0RFQlVHKQ0KICAg"
    "IHsNCiAgICAgICAgc2V0X2RlYnVnX2luZm9fZW5hYmxlZChmYWxzZSk7DQogICAgfQ0KICAgIHNldF9h"
    "ZnRlcl9wcmVwYXJlX2NhbGxiYWNrKHNldHVwX2RlZmF1bHRfZmxhZ3MpOw0KICAgIHJldHVybiBleGVj"
    "dXRlKCk7DQp9";
// Generated by bin2c, don't edit!
// clang-format off
#include <stddef.h>
#include <stdint.h>

size_t bin2c_test_c_size = 101;
char const bin2c_test_c[] =
    "dHlwZWRlZiBzdHJ1Y3QgVGVzdEVudHJ5IFRlc3RFbnRyeTsNClRlc3RFbnRyeSogdGVzdF9lbnRyaWVz"
    "ID0gKHZvaWQqKTA7DQppbnQgbnVtX3Rlc3RfZW50cmllcyA9IDA7DQo=";
// Generated by bin2c, don't edit!
// clang-format off
#include <stddef.h>
#include <stdint.h>

size_t bin2c_test_h_size = 1998;
char const bin2c_test_h[] =
    "I3ByYWdtYSBvbmNlDQoNCiNpbmNsdWRlIDxzdGRib29sLmg+DQojaW5jbHVkZSA8c3RkaW8uaD4NCiNp"
    "bmNsdWRlIDxzdGRsaWIuaD4NCg0KdHlwZWRlZiB2b2lkIEZuVGVzdEVudHJ5KCk7DQoNCnR5cGVkZWYg"
    "c3RydWN0IFRlc3RFbnRyeQ0Kew0KICAgIGNoYXIgY29uc3QqIG5hbWU7DQogICAgY2hhciBjb25zdCog"
    "Z3JvdXA7DQogICAgRm5UZXN0RW50cnkqIGZuOw0KICAgIGNoYXIgY29uc3QqIGZpbGU7DQogICAgaW50"
    "IGxpbmU7DQp9IFRlc3RFbnRyeTsNCg0KI2lmZGVmIEJVSUxEX1RFU1QNCiNpbmNsdWRlIDxzdGRsaWIu"
    "aD4NCiNpZmRlZiBfX2NwbHVzcGx1cw0KZXh0ZXJuICJDIiBUZXN0RW50cnkqIHRlc3RfZW50cmllczsN"
    "CmV4dGVybiAiQyIgaW50IG51bV90ZXN0X2VudHJpZXM7DQojZWxzZQ0KZXh0ZXJuIFRlc3RFbnRyeSog"
    "dGVzdF9lbnRyaWVzOw0KZXh0ZXJuIGludCBudW1fdGVzdF9lbnRyaWVzOw0KI2VuZGlmDQpzdGF0aWMg"
    "aW5saW5lIHZvaWQgdGVzdF9wdXNoX2VudHJ5KFRlc3RFbnRyeSBlbnRyeSkNCnsNCiAgICB0ZXN0X2Vu"
    "dHJpZXMgPSAoVGVzdEVudHJ5KilyZWFsbG9jKHRlc3RfZW50cmllcywgKG51bV90ZXN0X2VudHJpZXMg"
    "KyAxKSAqIHNpemVvZihUZXN0RW50cnkpKTsNCiAgICB0ZXN0X2VudHJpZXNbbnVtX3Rlc3RfZW50cmll"
    "cysrXSA9IGVudHJ5Ow0KfQ0KI2lmbmRlZiBDT05TVFJVQ1RPUg0KI2lmIGRlZmluZWQoX01TQ19WRVIp"
    "DQojZGVmaW5lIENPTlNUUlVDVE9SKG5hbWUpICAgICAgICAgICAgICAgICAgIFwNCiAgICBfUHJhZ21h"
    "KCJzZWN0aW9uKFwiLkNSVCRYQ1VcIiwgcmVhZCkiKTsgXA0KICAgIHN0YXRpYyB2b2lkIG5hbWUodm9p"
    "ZCk7ICAgICAgICAgICAgICAgICBcDQogICAgX19kZWNsc3BlYyhhbGxvY2F0ZSgiLkNSVCRYQ1UiKSkg"
    "dm9pZCAoKm5hbWUjI19wdHIpKHZvaWQpID0gbmFtZTsNCiNlbHNlDQojZGVmaW5lIENPTlNUUlVDVE9S"
    "KG5hbWUpIF9fYXR0cmlidXRlX18oKGNvbnN0cnVjdG9yKDEwMSkpKQ0KI2VuZGlmDQojZW5kaWYNCiNk"
    "ZWZpbmUgVEVTVF9SRUdJU1RFUihmbiwgLi4uKSAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg"
    "ICAgICBcDQogICAgc3RhdGljIHZvaWQgZm4odm9pZCk7ICAgICAgICAgICAgICAgICAgICAgICAgICAg"
    "ICAgICAgICAgICAgICAgXA0KICAgIENPTlNUUlVDVE9SKGZuIyNfcmVnaXN0ZXIpICAgICAgICAgICAg"
    "ICAgICAgICAgICAgICAgICAgICAgICAgIFwNCiAgICBzdGF0aWMgdm9pZCBmbiMjX3JlZ2lzdGVyKCkg"
    "ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBcDQogICAgeyAgICAgICAgICAgICAgICAg"
    "ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgXA0KICAgICAgICBUZXN0"
    "RW50cnkgZSA9IHsjZm4sICNfX1ZBX0FSR1NfXywgZm4sIF9fRklMRV9fLCBfX0xJTkVfX307IFwNCiAg"
    "ICAgICAgdGVzdF9wdXNoX2VudHJ5KGUpOyAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg"
    "ICAgICBcDQogICAgfQ0KI2Vsc2UNCiNkZWZpbmUgVEVTVF9SRUdJU1RFUihmbiwgLi4uKQ0KI2VuZGlm"
    "IC8vIEJVSUxEX1RFU1QNCg0KI2RlZmluZSBURVNUKGZuLCAuLi4pIFRFU1RfUkVHSVNURVIoZm4sICMj"
    "X19WQV9BUkdTX18pIHN0YXRpYyB2b2lkIGZuKHZvaWQpDQojZGVmaW5lIFRFU1RfRElTQUJMRUQoZm4s"
    "IC4uLikgdm9pZCBmbih2b2lkKQ0KDQpzdGF0aWMgaW5saW5lIHZvaWQgYXNzZXJ0X2ltcGwoYm9vbCBj"
    "b25kLCBjaGFyIGNvbnN0KiBtc2csIGNoYXIgY29uc3QqIGZpbGUsIGludCBsaW5lKQ0Kew0KICAgIGlm"
    "ICghY29uZCkNCiAgICB7DQogICAgICAgIGZwcmludGYoc3RkZXJyLCAiICBBU1NFUlQgRkFJTEVEOiAl"
    "czolZDogJXNcbiIsIGZpbGUsIGxpbmUsIG1zZyk7DQogICAgICAgIGV4aXQoRVhJVF9GQUlMVVJFKTsN"
    "CiAgICB9DQp9DQoNCiNkZWZpbmUgQVNTRVJUKGNvbmQpIGFzc2VydF9pbXBsKGNvbmQsICNjb25kLCBf"
    "X0ZJTEVfXywgX19MSU5FX18p";
// Generated by bin2c, don't edit!
// clang-format off
#include <stddef.h>
#include <stdint.h>

size_t bin2c_test_main_c_size = 4868;
char const bin2c_test_main_c[] =
    "I2luY2x1ZGUgInRlc3QuaCINCg0KI2luY2x1ZGUgPGFzc2VydC5oPg0KI2luY2x1ZGUgPHN0ZGlvLmg+"
    "DQojaW5jbHVkZSA8c3RyaW5nLmg+DQoNCmV4dGVybiBUZXN0RW50cnkqIHRlc3RfZW50cmllczsNCmV4"
    "dGVybiBpbnQgbnVtX3Rlc3RfZW50cmllczsNCg0KY2hhciBjb25zdCogY3VyX2V4ZV9wYXRoOw0KDQpz"
    "dGF0aWMgRklMRSogb3NfcG9wZW4oY2hhciBjb25zdCogY21kLCBjaGFyIGNvbnN0KiBtb2RlKQ0Kew0K"
    "I2lmZGVmIF9XSU4zMg0KICAgIHJldHVybiBfcG9wZW4oY21kLCBtb2RlKTsNCiNlbHNlDQogICAgcmV0"
    "dXJuIHBvcGVuKGNtZCwgbW9kZSk7DQojZW5kaWYNCn0NCg0Kc3RhdGljIGludCBvc19wY2xvc2UoRklM"
    "RSogZmlsZSkNCnsNCiNpZmRlZiBfV0lOMzINCiAgICByZXR1cm4gX3BjbG9zZShmaWxlKTsNCiNlbHNl"
    "DQogICAgcmV0dXJuIHBjbG9zZShmaWxlKTsNCiNlbmRpZg0KfQ0KDQpzdGF0aWMgdm9pZCBwcmludF9l"
    "bnRyaWVzKHZvaWQpDQp7DQogICAgcHV0Y2hhcignWycpOw0KICAgIGludCBiX2ZpcnN0ID0gMTsNCiAg"
    "ICBmb3IgKGludCBpID0gMDsgaSAhPSBudW1fdGVzdF9lbnRyaWVzOyBpKyspDQogICAgew0KICAgICAg"
    "ICBUZXN0RW50cnkgY29uc3QqIGUgPSAmdGVzdF9lbnRyaWVzW2ldOw0KICAgICAgICBpZiAoYl9maXJz"
    "dCkNCiAgICAgICAgew0KICAgICAgICAgICAgYl9maXJzdCA9IDA7DQogICAgICAgIH0NCiAgICAgICAg"
    "ZWxzZQ0KICAgICAgICB7DQogICAgICAgICAgICBwdXRjaGFyKCcsJyk7DQogICAgICAgIH0NCiAgICAg"
    "ICAgcHV0cygiXG4gICAgeyIpOw0KICAgICAgICBwcmludGYoIiAgICAgICAgXCJuYW1lXCI6IFwiJXNc"
    "IixcbiIsIGUtPm5hbWUpOw0KICAgICAgICBwcmludGYoIiAgICAgICAgXCJncm91cFwiOiBcIiVzXCIs"
    "XG4iLCBlLT5ncm91cCk7DQogICAgICAgIHByaW50ZigiICAgICAgICBcImZpbGVcIjogXCIlc1wiLFxu"
    "IiwgZS0+ZmlsZSk7DQogICAgICAgIHByaW50ZigiICAgICAgICBcImxpbmVcIjogJWRcbiIsIGUtPmxp"
    "bmUpOw0KICAgICAgICBwcmludGYoIiAgICB9Iik7DQogICAgfQ0KICAgIHB1dHMoIlxuXSIpOw0KfQ0K"
    "DQpzdGF0aWMgdm9pZCBydW5fdGVzdChjaGFyIGNvbnN0KiBuYW1lKQ0Kew0KICAgIGZvciAoaW50IGkg"
    "PSAwOyBpICE9IG51bV90ZXN0X2VudHJpZXM7IGkrKykNCiAgICB7DQogICAgICAgIFRlc3RFbnRyeSBj"
    "b25zdCogZSA9ICZ0ZXN0X2VudHJpZXNbaV07DQogICAgICAgIGlmIChzdHJjbXAoZS0+bmFtZSwgbmFt"
    "ZSkgPT0gMCkNCiAgICAgICAgew0KICAgICAgICAgICAgZS0+Zm4oKTsNCiAgICAgICAgICAgIGJyZWFr"
    "Ow0KICAgICAgICB9DQogICAgfQ0KfQ0KDQojZGVmaW5lIE1BWF9KT0JTIDgNCg0Kc3RhdGljIGJvb2wg"
    "cnVuX2FsbF90ZXN0cygpDQp7DQogICAgc3RydWN0DQogICAgew0KICAgICAgICBGSUxFKiBwaXBlOw0K"
    "ICAgICAgICBzaXplX3Qgb3V0cHV0X3NpemU7DQogICAgICAgIHNpemVfdCBvdXRwdXRfY2FwYWNpdHk7"
    "DQogICAgICAgIGNoYXIgY29uc3QqIGZpbGU7DQogICAgICAgIGludCBsaW5lOw0KICAgICAgICBjaGFy"
    "KiBvdXRwdXQ7DQogICAgfSBzbG90c1tNQVhfSk9CU10gPSB7MH07DQogICAgc2l6ZV90IG5leHRfdGVz"
    "dCA9IDA7DQogICAgc2l6ZV90IG51bV9ydW5uaW5nID0gMDsNCiAgICBib29sIGhhc19lcnJvciA9IGZh"
    "bHNlOw0KICAgIHdoaWxlICh0cnVlKQ0KICAgIHsNCiAgICAgICAgd2hpbGUgKG51bV9ydW5uaW5nICE9"
    "IE1BWF9KT0JTICYmIG5leHRfdGVzdCAhPSAoc2l6ZV90KW51bV90ZXN0X2VudHJpZXMpDQogICAgICAg"
    "IHsNCiAgICAgICAgICAgIHNpemVfdCBpID0gLTE7DQogICAgICAgICAgICB3aGlsZSAoc2xvdHNbKytp"
    "XS5waXBlKTsNCiAgICAgICAgICAgIGFzc2VydChpICE9IE1BWF9KT0JTKTsNCiAgICAgICAgICAgIGNo"
    "YXIgY21kYnVmZmVyWzQwOTZdOw0KICAgICAgICAgICAgVGVzdEVudHJ5KiBlID0gJnRlc3RfZW50cmll"
    "c1tuZXh0X3Rlc3QrK107DQogICAgICAgICAgICBzbnByaW50ZihjbWRidWZmZXIsIHNpemVvZihjbWRi"
    "dWZmZXIpLCAiXCIlc1wiICVzIDI+JjEiLCBjdXJfZXhlX3BhdGgsIGUtPm5hbWUpOw0KICAgICAgICAg"
    "ICAgc2xvdHNbaV0ucGlwZSA9IG9zX3BvcGVuKGNtZGJ1ZmZlciwgInIiKTsNCiAgICAgICAgICAgIHNs"
    "b3RzW2ldLmZpbGUgPSBlLT5maWxlOw0KICAgICAgICAgICAgc2xvdHNbaV0ubGluZSA9IGUtPmxpbmU7"
    "DQogICAgICAgICAgICBzbG90c1tpXS5vdXRwdXRfc2l6ZSA9IDA7DQogICAgICAgICAgICBzbG90c1tp"
    "XS5vdXRwdXRfY2FwYWNpdHkgPSA0MDk2Ow0KICAgICAgICAgICAgc2xvdHNbaV0ub3V0cHV0ID0gbWFs"
    "bG9jKHNsb3RzW2ldLm91dHB1dF9jYXBhY2l0eSk7DQogICAgICAgICAgICBhc3NlcnQoc2xvdHNbaV0u"
    "b3V0cHV0KTsNCiAgICAgICAgICAgIG51bV9ydW5uaW5nICs9IDE7DQogICAgICAgIH0NCiAgICAgICAg"
    "Zm9yIChzaXplX3QgaSA9IDA7IGkgIT0gTUFYX0pPQlM7IGkrKykNCiAgICAgICAgew0KICAgICAgICAg"
    "ICAgaWYgKHNsb3RzW2ldLnBpcGUpDQogICAgICAgICAgICB7DQogICAgICAgICAgICAgICAgY2hhciBi"
    "dWZmZXJbMTAyNF07DQogICAgICAgICAgICAgICAgc2l6ZV90IG4gPSBmcmVhZChidWZmZXIsIDEsIHNp"
    "emVvZihidWZmZXIpLCBzbG90c1tpXS5waXBlKTsNCiAgICAgICAgICAgICAgICBpZiAobikNCiAgICAg"
    "ICAgICAgICAgICB7DQogICAgICAgICAgICAgICAgICAgIGlmIChzbG90c1tpXS5vdXRwdXRfc2l6ZSAr"
    "IG4gPj0gc2xvdHNbaV0ub3V0cHV0X2NhcGFjaXR5KQ0KICAgICAgICAgICAgICAgICAgICB7DQogICAg"
    "ICAgICAgICAgICAgICAgICAgICBzbG90c1tpXS5vdXRwdXRfY2FwYWNpdHkgPSBzbG90c1tpXS5vdXRw"
    "dXRfY2FwYWNpdHkgKiAyICsgbjsNCiAgICAgICAgICAgICAgICAgICAgICAgIHNsb3RzW2ldLm91dHB1"
    "dCA9IHJlYWxsb2Moc2xvdHNbaV0ub3V0cHV0LCBzbG90c1tpXS5vdXRwdXRfY2FwYWNpdHkpOw0KICAg"
    "ICAgICAgICAgICAgICAgICAgICAgYXNzZXJ0KHNsb3RzW2ldLm91dHB1dCk7DQogICAgICAgICAgICAg"
    "ICAgICAgIH0NCiAgICAgICAgICAgICAgICAgICAgbWVtY3B5KHNsb3RzW2ldLm91dHB1dCArIHNsb3Rz"
    "W2ldLm91dHB1dF9zaXplLCBidWZmZXIsIG4pOw0KICAgICAgICAgICAgICAgICAgICBzbG90c1tpXS5v"
    "dXRwdXRfc2l6ZSArPSBuOw0KICAgICAgICAgICAgICAgIH0NCiAgICAgICAgICAgICAgICBpZiAoZmVv"
    "ZihzbG90c1tpXS5waXBlKSkNCiAgICAgICAgICAgICAgICB7DQogICAgICAgICAgICAgICAgICAgIGlu"
    "dCBleGl0X2NvZGUgPSBvc19wY2xvc2Uoc2xvdHNbaV0ucGlwZSk7DQogICAgICAgICAgICAgICAgICAg"
    "IHNsb3RzW2ldLnBpcGUgPSBOVUxMOw0KICAgICAgICAgICAgICAgICAgICBpZiAoIWhhc19lcnJvciAm"
    "JiBleGl0X2NvZGUgIT0gRVhJVF9TVUNDRVNTKQ0KICAgICAgICAgICAgICAgICAgICB7DQogICAgICAg"
    "ICAgICAgICAgICAgICAgICBoYXNfZXJyb3IgPSB0cnVlOw0KICAgICAgICAgICAgICAgICAgICB9DQog"
    "ICAgICAgICAgICAgICAgICAgIHByaW50ZigiVEVTVCAlczolZCBFWElUOiAlZFxuIiwgc2xvdHNbaV0u"
    "ZmlsZSwgc2xvdHNbaV0ubGluZSwgZXhpdF9jb2RlKTsNCiAgICAgICAgICAgICAgICAgICAgaWYgKHNs"
    "b3RzW2ldLm91dHB1dF9zaXplKQ0KICAgICAgICAgICAgICAgICAgICB7DQogICAgICAgICAgICAgICAg"
    "ICAgICAgICBwcmludGYoIk9VVFBVVDpcbiIpOw0KICAgICAgICAgICAgICAgICAgICAgICAgcHJpbnRm"
    "KCIlLipzXG4iLCAoaW50KXNsb3RzW2ldLm91dHB1dF9zaXplLCBzbG90c1tpXS5vdXRwdXQpOw0KICAg"
    "ICAgICAgICAgICAgICAgICB9DQogICAgICAgICAgICAgICAgICAgIGZyZWUoc2xvdHNbaV0ub3V0cHV0"
    "KTsNCiAgICAgICAgICAgICAgICAgICAgbnVtX3J1bm5pbmcgLT0gMTsNCiAgICAgICAgICAgICAgICB9"
    "DQogICAgICAgICAgICB9DQogICAgICAgIH0NCiAgICAgICAgaWYgKG51bV9ydW5uaW5nID09IDAgJiYg"
    "bmV4dF90ZXN0ID09IChzaXplX3QpbnVtX3Rlc3RfZW50cmllcykNCiAgICAgICAgew0KICAgICAgICAg"
    "ICAgYnJlYWs7DQogICAgICAgIH0NCiAgICB9DQogICAgcmV0dXJuIGhhc19lcnJvcjsNCn0NCg0KI2lm"
    "ZGVmIF9NU0NfVkVSDQojaW5jbHVkZSA8Y3J0ZGJnLmg+DQojZW5kaWYNCg0KaW50IG1haW4oaW50IGFy"
    "Z2MsIGNoYXIqKiBhcmd2KQ0Kew0KI2lmZGVmIF9NU0NfVkVSDQogICAgX3NldF9hYm9ydF9iZWhhdmlv"
    "cigwLCBfV1JJVEVfQUJPUlRfTVNHIHwgX0NBTExfUkVQT1JURkFVTFQpOw0KICAgIF9DcnRTZXRSZXBv"
    "cnRNb2RlKF9DUlRfQVNTRVJULCAwKTsNCiAgICBfQ3J0U2V0UmVwb3J0TW9kZShfQ1JUX0VSUk9SLCAw"
    "KTsNCiAgICBfQ3J0U2V0UmVwb3J0TW9kZShfQ1JUX1dBUk4sIDApOw0KI2VuZGlmDQoNCiAgICBjdXJf"
    "ZXhlX3BhdGggPSBhcmd2WzBdOw0KICAgIGlmIChhcmdjID09IDEpDQogICAgew0KICAgICAgICBpZiAo"
    "cnVuX2FsbF90ZXN0cygpKQ0KICAgICAgICB7DQogICAgICAgICAgICByZXR1cm4gRVhJVF9GQUlMVVJF"
    "Ow0KICAgICAgICB9DQogICAgICAgIGVsc2UNCiAgICAgICAgew0KICAgICAgICAgICAgcmV0dXJuIEVY"
    "SVRfU1VDQ0VTUzsNCiAgICAgICAgfQ0KICAgIH0NCiAgICBpZiAoYXJnYyA9PSAyKQ0KICAgIHsNCiAg"
    "ICAgICAgaWYgKHN0cmNtcChhcmd2WzFdLCAiLXByaW50IikgPT0gMCkNCiAgICAgICAgew0KICAgICAg"
    "ICAgICAgcHJpbnRfZW50cmllcygpOw0KICAgICAgICAgICAgcmV0dXJuIDA7DQogICAgICAgIH0NCiAg"
    "ICAgICAgcnVuX3Rlc3QoYXJndlsxXSk7DQogICAgICAgIHJldHVybiAwOw0KICAgIH0NCiAgICByZXR1"
    "cm4gMTsNCn0=";

#include <stdlib.h>
#include <string.h>

static int gen_embedded_files_cb(Node* node)
{
    struct EmbeddedFile* e = node->ctx;
    Node* output = FILE(e->path);
    FILE* file = os_fopen(output->path, "wb");
    if (!file)
    {
        printf("cannot open file: %s\n", output->path);
        return EXIT_FAILURE;
    }
    uint8_t* buf = allocator_malloc(allocator_temp(), *e->size);
    int n = base64_decode(e->base64, strlen(e->base64), buf, *e->size);
    fwrite(buf, 1, n, file);
    fclose(file);
    return EXIT_SUCCESS;
}

Node* create_gen_embedded_file_cmd(EmbeddedFile* embedded_file)
{
    extern bool b_dll_mode;
    Node* cmd = CALLBACK_CMD(gen_embedded_files_cb, embedded_file);
    cmd_set_source_location(cmd, __FILE__, __LINE__);
    uint32_t type = node_make_file_type(embedded_file->type, 0);
    Node* output = get_or_add_node(type, fmt(embedded_file->path), embedded_file->struct_bytes);
    output->file_type = embedded_file->type;
    cmd_add_output(cmd, output);
    if (b_dll_mode)
    {
        Node* self = get_or_add_file_with_type(fmt("{self}"), FILE_TYPE_EXE);
        cmd_add_input(cmd, self);
    }
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));
    return cmd;
}

ENTRY(gen_build_c)
{
    extern size_t bin2c_build_script_tpl_c_size;
    extern char const bin2c_build_script_tpl_c[];

    if (os_file_exists("build.c"))
    {
        return;
    }

    static struct EmbeddedFile file = {
        .base64 = bin2c_build_script_tpl_c,
        .size = &bin2c_build_script_tpl_c_size,
        .path = "build.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    };
    create_gen_embedded_file_cmd(&file);
}

#include <stdint.h>
#include <stdlib.h>

extern size_t bin2c_test_h_size;
extern char const bin2c_test_h[];
extern size_t bin2c_test_c_size;
extern char const bin2c_test_c[];
extern size_t bin2c_test_main_c_size;
extern char const bin2c_test_main_c[];

static struct EmbeddedFile embedded_files[] = {
    {
        .base64 = bin2c_test_h,
        .size = &bin2c_test_h_size,
        .path = "{out_dir}/cup/test.h",
        .type = FILE_TYPE_NORMAL,
        .struct_bytes = sizeof(Node),
    },
    {
        .base64 = bin2c_test_c,
        .size = &bin2c_test_c_size,
        .path = "{out_dir}/cup/test.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    },
    {
        .base64 = bin2c_test_main_c,
        .size = &bin2c_test_main_c_size,
        .path = "{out_dir}/cup/test_main.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    },
};

void gen_test_src(void)
{
    for (size_t i = 0; i != static_array_size(embedded_files); i++)
    {
        create_gen_embedded_file_cmd(&embedded_files[i]);
    }
}


static void after_self_built(Node* cmd)
{
    cmd_after_execute(cmd);
    restart();
}

ENTRY(build_self_header_only)
{
    extern char const* cup_h_dir;
    extern char** build_script_search_directories;
    void collect_build_scripts(char const* directory, Allocator* allocator);
    Allocator* allocator = allocator_temp();
    add_build_script("build.c");
    for (size_t i = 0; i != array_size(build_script_search_directories); i++)
    {
        collect_build_scripts(build_script_search_directories[i], allocator);
    }
    Node* self = EXE("{self_name}");
    Node* cmd_link = LINK(self);
    link_cmd_setup_self_build(cmd_link);
    {
        if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
        {
            Node* pdb = FILE("{out_dir}/cup.exe.pdb");
            link_cmd_set_pdb(cmd_link, pdb);
            if (default_toolchain == TOOLCHAIN_TYPE_GCC)
            {
                link_cmd_add_lib(cmd_link, "userenv");
                link_cmd_add_lib(cmd_link, "bcrypt");
            }
        }
        extern StringSet* build_scripts;
        for (uint32_t i = build_scripts->begin; i != build_scripts->end; i = hash_next(build_scripts, i))
        {
            Node* src = SRC(hash_key(build_scripts, i));
            Node* obj = OBJ(src);
            Node* cc = CC(src, obj);
            if (string_equal(src->path, "build.c"))
            {
                c_compile_cmd_add_define(cc, "BUILD_IMPLEMENTATION");
            }
            c_compile_cmd_add_self_build_options(cc);
            if (cup_h_dir)
            {
                c_compile_cmd_add_include_directory(cc, cup_h_dir);
            }
            if (CURRENT_PLATFORM == PLATFORM_LINUX)
            {
                c_compile_cmd_add_define(cc, "_GNU_SOURCE");
            }
            link_cmd_add_input(cmd_link, obj);
        }

        cmd_set_after_execute_fn(cmd_link, after_self_built);
    }
}

static void bootstrap_compile_link_make_cmdline_llvm_gcc_zig(Node* node, Node* out_exe)
{
    ToolchainType toolchain = default_toolchain;
    char const* compiler = (toolchain == TOOLCHAIN_TYPE_GCC ? "gcc" : (toolchain == TOOLCHAIN_TYPE_LLVM ? "clang" : "zig cc"));
    Node* cup_h = FILE("cup.h");
    cmd_add_option(node, NULL, compiler, OPTION_EXE);
    cmd_add_option(node, "-o ", out_exe->path, OPTION_OUTPUT);
    cmd_add_option(node, "-x c ", cup_h->path, OPTION_INPUT);
    cmd_add_option(node, "-D", "MAIN_ENTRY", OPTION_FLAG);
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        cmd_add_option(node, "-D", "_GNU_SOURCE", OPTION_FLAG);
        cmd_add_option(node, "-fms-extensions", NULL, OPTION_FLAG);
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (default_toolchain == TOOLCHAIN_TYPE_GCC)
        {
            cmd_add_option(node, "-l", "userenv", OPTION_FLAG);
            cmd_add_option(node, "-l", "bcrypt", OPTION_FLAG);
        }
    }
    cmd_add_input(node, cup_h);
    cmd_add_output(node, out_exe);
}

static void bootstrap_compile_link_msvc_write_stdout_line_fn(Node* node, char const* line)
{
    if (string_equal(line, "cup.h"))
    {
        return;
    }
    cmd_write_stderr_line(node, line);
}

static void bootstrap_compile_link_make_cmdline_msvc(Node* node, Node* out_exe)
{
    extern Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch);

    Node* env = msvc_get_env_node(default_toolchain, CURRENT_ARCHITECTURE);
    cmd_set_env(node, env);
    Node* cup_h = FILE("cup.h");
    Node* pdb = FILE("{out_dir}/{}.pdb", out_exe->path);
    node->ctx = pdb;
    cmd_add_option(node, NULL, "cl", OPTION_EXE);
    cmd_add_option(node, "/Fe:", out_exe->path, OPTION_OUTPUT);
    cmd_add_option(node, "/Tc ", cup_h->path, OPTION_INPUT);
    cmd_add_option(node, "/std:", "clatest", OPTION_FLAG);
    cmd_add_option(node, "/D", "MAIN_ENTRY", OPTION_BRIGHT_FLAG);
    cmd_add_option(node, "/Od", NULL, OPTION_FLAG);
    cmd_add_option(node, "/nologo", NULL, OPTION_FLAG);
    cmd_add_option(node, "/Z7", NULL, OPTION_FLAG);
    cmd_add_option(node, "/link", NULL, OPTION_FLAG);
    cmd_add_option(node, "/debug", NULL, OPTION_FLAG);
    cmd_add_option(node, "/incremental:", "no", OPTION_FLAG);
    cmd_add_option(node, "/noexp", NULL, OPTION_FLAG);
    cmd_add_option(node, "/noimplib", NULL, OPTION_FLAG);
    cmd_add_option(node, "/pdb:", pdb->path, OPTION_OUTPUT);
    cmd_add_input(node, cup_h);
    cmd_add_output(node, out_exe);
    cmd_add_output(node, pdb);
    node->write_stdout_line_fn = bootstrap_compile_link_msvc_write_stdout_line_fn;
    node->write_stderr_line_fn = bootstrap_compile_link_msvc_write_stdout_line_fn;
}

int build_self(void);
ToolchainType c_toolchain_select_toolchain_automatically();

static void bootstrap_compile_link_cmd_before_execute(Node* node)
{
    void c_toolchain_rename_to_old(char const* path);
    Node* output = node->extra_data;
    Node* pdb = node->ctx;
    if (!os_file_writable(output->path))
    {
        c_toolchain_rename_to_old(output->path);
    }
    if (pdb && !os_file_writable(pdb->path))
    {
        c_toolchain_rename_to_old(pdb->path);
    }
    cmd_before_execute(node);
}

int bootstrap(void)
{
    Node* self = EXE("{self_name}");
    Node* cmd = CMD(NULL);
    ToolchainType toolchain = c_toolchain_select_toolchain_automatically();
    set_default_toolchain(toolchain);
    if (toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        bootstrap_compile_link_make_cmdline_msvc(cmd, self);
    }
    else if (toolchain == TOOLCHAIN_TYPE_GCC || toolchain == TOOLCHAIN_TYPE_LLVM || toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        bootstrap_compile_link_make_cmdline_llvm_gcc_zig(cmd, self);
    }
    cmd->extra_data = self;
    cmd->before_execute = bootstrap_compile_link_cmd_before_execute;
    node_ensure_prepared(cmd);
    return build_self();
}

void init_mode(void)
{
    set_var("test_src_dir", get_var("out_dir"));

    extern bool b_bootstrap;
    if (b_bootstrap)
    {
        int exit_code = bootstrap();
        exit(exit_code);
    }
}

// Compile commands:
// Windows:
// clang -x c cup.h -DMAIN_ENTRY -o cup.exe
// gcc -x c cup.h -DMAIN_ENTRY -luserenv -lbcrypt -o cup.exe
// cl /Fe:cup.exe /Tc cup.h /std:clatest /DMAIN_ENTRY
// cl /Fe:cup.exe /Tc cup.h /std:clatest /DMAIN_ENTRY /Od /nologo /Z7 /link /debug /incremental:no /noexp /noimplib /pdb:build/cup.exe.pdb
// Linux/macOS(-D_GNU_SOURCE can be omitted on macOS):
// gcc -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions
// clang -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions

#ifdef MAIN_ENTRY
int main(int argc, char** argv)
{
    set_default_toolchain(get_toolchain_by_current_compiler());
    OptimizationType optimization = OPTIMIZATION_TYPE_DEBUG;
    set_default_optimization(optimization);
    if (optimization != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    return execute();
}
#endif
#endif // defined(BUILD_IMPLEMENTATION) || defined(MAIN_ENTRY)
#endif // CUP_H
