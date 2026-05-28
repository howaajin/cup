#include "core/allocator.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/string.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char const path_prefix[] = "src/";

#define report_error(fmt, ...)         \
    fprintf(stderr, fmt, __VA_ARGS__); \
    exit(1)

void skip_bom(FILE* file)
{
    uint8_t ch[3];
    size_t n = fread(ch, 1, 3, file);
    if (n != 3)
    {
        return;
    }
    if (ch[0] != 0xEF || ch[1] != 0xBB || ch[2] != 0xBF)
    {
        fseek(file, 0, SEEK_SET);
    }
}

char const* skip_space(char const* p)
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

bool match_pragma_once(char const* line)
{
    char const* p = line;
    p = skip_space(p);
    if (*p != '#')
    {
        return false;
    }
    ++p;
    p = skip_space(p);
    char const pragma[] = "pragma";
    if (memcmp(p, pragma, sizeof(pragma) - 1) != 0)
    {
        return false;
    }
    p += sizeof(pragma) - 1;
    p = skip_space(p);
    char const once[] = "once";
    if (memcmp(p, once, sizeof(once) - 1) != 0)
    {
        return false;
    }
    return true;
}

void get_line(FILE* file, Allocator* allocator, char** line)
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
}

bool match_include_quote(char const* line, char** include, char** ext, char** end)
{
    char const* p = line;
    p = skip_space(p);
    if (*p != '#')
    {
        return false;
    }
    ++p;
    p = skip_space(p);
    char const inc_str[] = "include";
    if (memcmp(p, inc_str, sizeof(inc_str) - 1) != 0)
    {
        return false;
    }
    p += sizeof(inc_str) - 1;
    p = skip_space(p);
    if (*p != '"')
    {
        return false;
    }
    ++p;
    *include = (char*)p;
    while (*p)
    {
        if (*p == '"')
        {
            *end = (char*)p;
            break;
        }
        if (*p == '.')
        {
            *ext = (char*)(p + 1);
        }
        ++p;
    }
    return true;
}

void merge_header(Allocator* allocator, char* path, StringHash* set, char** merged, bool b_is_source)
{
    bool b_existed;
    hash_insert_check(set, path, &b_existed);
    if (b_existed)
    {
        array_free(allocator, path);
        return;
    }
    Allocator* temp_allocator = allocator_temp();
    char* real_path = string_from_print(temp_allocator, "%s%s", path_prefix, path);
    fprintf(stdout, "%s\n", real_path);
    FILE* file = fopen(real_path, "rb");
    if (file == NULL)
    {
        report_error("cannot open file: %s\n", real_path);
    }
    skip_bom(file);
    char* line = string_new(temp_allocator, 0, NULL);
    while (true)
    {
        array_resize(temp_allocator, line, 0);
        get_line(file, temp_allocator, &line);
        if (array_size(line) == 0)
        {
            break;
        }
        bool b_skip_line = false;
        char* p;
        char* ext = NULL;
        char* end = NULL;
        if (match_include_quote(line, &p, &ext, &end))
        {
            if (!memcmp(ext, "inl", 3) || *ext == 'h' || *ext == 'c')
            {
                b_skip_line = true;
                *end = '\0';
                char* include = string_from_c_str(allocator, p);
                merge_header(allocator, include, set, merged, memcmp(ext, "c", 1) == 0);
            }
        }
        else if (match_pragma_once(line))
        {
            b_skip_line = true;
        }
        if (!b_is_source && !b_skip_line)
        {
            string_printf(allocator, *merged, "%s", line);
        }
    }
}

void merge_source(Allocator* allocator, char const* path, char** merged)
{
    Allocator* temp_allocator = allocator_temp();
    char* real_path = string_from_print(temp_allocator, "%s%s", path_prefix, path);
    FILE* file = fopen(real_path, "rb");
    if (file == NULL)
    {
        report_error("cannot open file: %s", real_path);
    }
    skip_bom(file);
    char* line = NULL;
    while (true)
    {
        array_resize(temp_allocator, line, 0);
        get_line(file, temp_allocator, &line);
        if (array_size(line) == 0)
        {
            break;
        }
        bool b_skip_line = false;
        char* include = NULL;
        char* ext = NULL;
        char* end = NULL;
        if (match_include_quote(line, &include, &ext, &end))
        {
            if (!memcmp(ext, "inl", 3) || *ext == 'h' || *ext == 'c')
            {
                b_skip_line = true;
                if (*ext == 'c')
                {
                    *end = '\0';
                    merge_source(allocator, include, merged);
                }
            }
        }
        if (!b_skip_line)
        {
            string_printf(allocator, *merged, "%s", line);
        }
    }
}

int main(int argc, char** argv)
{
    bool b_no_impl = false;
    char const* out_path = NULL;
    char const* input = NULL;
    char const* program = *argv;
    while (*argv)
    {
        if (string_equal("-h", *argv))
        {
            b_no_impl = true;
        }
        else if (string_equal("-o", *argv))
        {
            argv++;
            out_path = *argv;
        }
        else if (*argv != program && (*argv)[0] != '-')
        {
            input = *argv;
        }
        argv++;
    }
    if (out_path == NULL || input == NULL)
    {
        return EXIT_FAILURE;
    }
    Allocator* allocator = allocator_create_chained();
    StringHash set = {.allocator = allocator};
    char* merged = NULL;
    string_printf(allocator, merged, "#ifndef CUP_H\n");
    char* include = string_from_c_str(allocator, input);
    merge_header(allocator, include, &set, &merged, true);

    if (!b_no_impl)
    {
        string_printf(allocator, merged, "\n\n#if defined(BUILD_IMPLEMENTATION) || defined(MAIN_ENTRY)\n");
        merge_source(allocator, include, &merged);
        string_printf(allocator, merged, "#endif // defined(BUILD_IMPLEMENTATION) || defined(MAIN_ENTRY)\n");
    }
    string_printf(allocator, merged, "#endif // CUP_H\n");

    FILE* outfile = fopen(out_path, "wb");
    fwrite(merged, 1, array_size(merged), outfile);
    fclose(outfile);
    allocator_destroy(allocator);
    return EXIT_SUCCESS;
}