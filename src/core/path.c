#include "core/path.h"
#include "core/allocator.h"
#include "core/array.h"
#include "core/string.h"
#include "core/macros.h"

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
    expect(p, "p is NULL");

    if (*p && *(p + 1) == ':')
    {
        return p + 2;
    }
    return p;
}

char const* path_skip_root_directory(char const* p)
{
    expect(p, "p is NULL");

    if (*p == '/' || *p == '\\')
    {
        return p + 1;
    }
    return p;
}

char const* path_skip_root_path(char const* p)
{
    expect(p, "p is NULL");
    p = path_skip_root_name(p);
    p = path_skip_root_directory(p);
    return p;
}

int path_root_name_length(char const* p)
{
    expect(p, "p is NULL");
    if (*p && *(p + 1) == ':')
    {
        return 2;
    }
    return 0;
}

char* path_root_name(char const* p, Allocator* allocator)
{
    expect(p, "p is NULL");
    int root_name_len = path_root_name_length(p);
    return string_from_print(allocator, "%.*s", root_name_len, p);
}

char* path_root_directory(char const* p, Allocator* allocator)
{
    expect(p, "p is NULL");
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
    expect(p, "p is NULL");
    Allocator* temp_allocator = allocator_temp();
    char* root_name = path_root_name(p, temp_allocator);
    char* root_dir = path_root_directory(p, temp_allocator);
    char* root_path = string_from_print(allocator, "%s%s", root_name, root_dir);
    return root_path;
}

char const* path_relative_path(char const* p)
{
    expect(p, "p is NULL");
    return path_skip_root_path(p);
}

bool path_has_relative_path(char const* p)
{
    expect(p, "p is NULL");
    char const* relative_path = path_skip_root_path(p);
    return *relative_path;
}

bool path_is_empty(char const* p)
{
    expect(p, "p is NULL");
    return *p == 0;
}

bool path_has_root_name(char const* p)
{
    expect(p, "p is NULL");
    if (*p && *(p + 1) == ':')
    {
        return true;
    }
    return false;
}

bool path_has_root_directory(char const* p)
{
    expect(p, "p is NULL");
    p = path_skip_root_name(p);
    if (*p == '/' || *p == '\\')
    {
        return true;
    }
    return false;
}

void path_parse_root_name(char const* p, struct PathElement* e)
{
    expect(p, "p is NULL");
    expect(e, "e is NULL");
    expect(e->allocator, "e->allocator is NULL");

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
    expect(p, "p is NULL");
    expect(e, "e is NULL");
    expect(e->allocator, "e->allocator is NULL");

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
    expect(p, "p is NULL");
    expect(e, "e is NULL");
    expect(e->allocator, "e->allocator is NULL");

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
    expect(p, "p is NULL");
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
    expect(p, "p is NULL");

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
    expect(path, "path is NULL");
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
    expect(path, "path is NULL");
    expect(ext, "ext is NULL");

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
    expect(path, "path is NULL");

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
        if (strcmp(n, ".") == 0)
        {
            continue;
        }
        if (strcmp(n, "..") == 0)
        {
            if (array_size(elements) != 0 && array_size(elements) && strcmp(array_last(elements), "..") != 0)
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
        string_append(allocator, result, elements[i]);
    }
    if (*result == 0)
    {
        string_putc(allocator, result, '.');
    }
    return result;
}

char* path_lexically_relative(char const* path, char const* base, Allocator* allocator)
{
    expect(path, "path is NULL");
    expect(base, "base is NULL");
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
    expect(path, "path is NULL");
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
    expect(parser && parser->cursor, "parser or parser->cursor is NULL");

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
