#include "core/array.h"
#include "core/os.h"

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
