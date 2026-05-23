#include "core/allocator.h"
#include "core/macros.h"
#include "core/string.h"
#include "cup/node.h"
#include "cup/var.h"

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
