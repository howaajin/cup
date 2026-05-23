#include "core/utilities.h"
#include "core/allocator.h"
#include "core/array.h"
#include "core/os.h"
#include "core/string.h"

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