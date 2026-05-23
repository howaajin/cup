#include "core/os.h"
#include "core/allocator.h"
#include "core/path.h"
#include "core/string.h"

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
