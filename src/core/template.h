#pragma once

#include "core/array.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STR_LEN 256

typedef struct Allocator Allocator;
void* allocator_realloc(Allocator* allocator, void* ptr, size_t size);
void* allocator_calloc(Allocator* allocator, size_t count, size_t size);

typedef enum template_value_e
{
    template_value_string,
    template_value_bool,
    template_value_array,
    template_value_dict,
} template_value_e;

typedef struct template_array_t template_array_t;
typedef struct template_dict_t template_dict_t;
typedef struct pair_t pair_t;
typedef struct value_t value_t;

typedef struct template_dict_t
{
    Allocator* allocator;
    pair_t* key_value_pairs;
} template_dict_t;

typedef struct template_array_t
{
    Allocator* allocator;
    value_t* values;
} template_array_t;

typedef struct value_t
{
    template_value_e type;
    union
    {
        char const* str;
        template_array_t array;
        template_dict_t dict;
        bool boolean;
    };
} value_t;

typedef struct pair_t
{
    char const* key;
    value_t value;
} pair_t;

typedef struct template_t
{
    Allocator* allocator;
    value_t value;
    FILE* input;
    FILE* output;
    long line_start;
    long begin_of_line;
    long out_line_start;
    bool b_skip;
} template_t;

static void template_dict_init(value_t* dict, Allocator* allocator)
{
    dict->type = template_value_dict;
    dict->dict.key_value_pairs = NULL;
    dict->dict.allocator = allocator;
}

static void template_array_init(value_t* array, Allocator* allocator)
{
    array->type = template_value_array;
    array->array.values = NULL;
    array->array.allocator = allocator;
}

static template_t* template_create(Allocator* allocator)
{
    template_t* t = allocator_calloc(allocator, 1, sizeof(template_t));
    t->allocator = allocator;
    template_dict_init(&t->value, allocator);
    return t;
}

static void template_peek(FILE* file, size_t n, void* buffer)
{
    long p = ftell(file);
    size_t num_read = fread(buffer, 1, n, file);
    if (num_read != n)
    {
        for (size_t i = num_read; i != n; i++)
        {
            ((char*)buffer)[i] = '\0';
        }
    }
    fseek(file, p, SEEK_SET);
}

static void template_eat(template_t* t, size_t n, void* buffer)
{
    for (uint64_t i = 0; i != n; i++)
    {
        char ch = getc(t->input);
        if (ch == '\n')
        {
            t->line_start = ftell(t->input);
            t->begin_of_line = -1;
        }
        else if (t->begin_of_line == -1)
        {
            if (ch != ' ' && ch != '\t' && ch != '\r')
            {
                t->begin_of_line = ftell(t->input) - 1;
            }
        }
        if (buffer)
        {
            ((char*)buffer)[i] = ch;
        }
    }
}

static void template_skip_spaces(template_t* t)
{
    while (1)
    {
        char ch;
        template_peek(t->input, 1, &ch);
        if (ch == ' ' || ch == '\t')
        {
            template_eat(t, 1, NULL);
        }
        else
        {
            break;
        }
    }
}

static void template_skip_one_space(template_t* t)
{
    char ch;
    template_peek(t->input, 1, &ch);
    if (ch == ' ' || ch == '\t')
    {
        template_eat(t, 1, NULL);
    }
}

static void template_skip_spaces_ln(template_t* t)
{
    while (1)
    {
        char ch;
        template_peek(t->input, 1, &ch);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        {
            template_eat(t, 1, NULL);
        }
        else
        {
            break;
        }
    }
}

static void template_skip_bom(template_t* t)
{
    uint8_t bom[3];
    template_peek(t->input, 3, bom);
    if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
    {
        template_eat(t, 3, NULL);
    }
}

static void template_read_string(template_t* t, void* buffer)
{
    uint64_t i = 0;
    while (true)
    {
        char ch;
        template_peek(t->input, 1, &ch);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
        {
            assert(i < MAX_STR_LEN);
            ((char*)buffer)[i] = ch;
            i++;
            template_eat(t, 1, NULL);
        }
        else
        {
            assert(i < MAX_STR_LEN);
            ((char*)buffer)[i] = '\0';
            break;
        }
    }
}

static void template_skip_until_block_comment_end(template_t* t)
{
    char ch[3];
    while (true)
    {
        template_peek(t->input, 3, ch);
        assert(ch[0] != 0);
        assert(ch[1] != 0);
        if (ch[0] == '*' && ch[1] == '/')
        {
            template_eat(t, 2, NULL);
            break;
        }
        if (ch[0] == '-' && ch[1] == '*' && ch[2] == '/')
        {
            template_eat(t, 3, NULL);
            template_skip_spaces_ln(t);
            break;
        }
        template_eat(t, 1, NULL);
    }
}

static bool template_try_read_end_mark(template_t* t)
{
    char ch[5];
    bool b_found = false;
    while (true)
    {
        template_peek(t->input, sizeof(ch), ch);
        if (ch[0] == 0)
        {
            break;
        }
        if (strncmp(ch, "/*$*/", sizeof(ch)) == 0)
        {
            long current_pos = ftell(t->input);
            template_eat(t, sizeof(ch), NULL);
            b_found = true;
            if (current_pos == t->begin_of_line)
            {
                fseek(t->output, t->out_line_start, SEEK_SET);
            }
            break;
        }
        template_eat(t, 1, NULL);
    }
    while (true)
    {
        template_peek(t->input, 1, ch);
        if (ch[0] == '\r' || ch[0] == '\n')
        {
            template_eat(t, 1, NULL);
        }
        else
        {
            break;
        }
    }
    return b_found;
}

static void template_skip_until_next_end_mark(template_t* t)
{
    if (!template_try_read_end_mark(t))
    {
        assert(false);
    }
}

static value_t* template_dict_get(template_dict_t const* dict, char const* name)
{
    for (uint64_t i = 0; i != array_size(dict->key_value_pairs); i++)
    {
        if (strcmp(dict->key_value_pairs[i].key, name) == 0)
        {
            return &dict->key_value_pairs[i].value;
        }
    }
    return NULL;
}

static void template_dict_add(template_dict_t* dict, char const* name, value_t const* value)
{
    pair_t pair = {name, *value};
    array_push(dict->allocator, dict->key_value_pairs, pair);
}

static void template_array_add(template_array_t* array, value_t const* value)
{
    array_push(array->allocator, array->values, *value);
}

static void template_foreach(template_t* t, char const* array_name);

static void template_block(template_t* t, template_dict_t const* dict);

static void template_if(template_t* t, template_dict_t const* dict, char const* value_name)
{
    value_t const* value = template_dict_get(dict, value_name);
    assert(value != NULL);
    bool b_ok;
    if (value->type == template_value_bool)
    {
        b_ok = value->boolean;
    }
    if (value->type == template_value_string)
    {
        b_ok = value->str != NULL;
    }
    else
    {
        fprintf(stderr, "Cannot convert value to boolean: %s", value_name);
        assert(false);
    }
    t->b_skip = !b_ok;
    template_block(t, dict);
    char ch[3];
    template_peek(t->input, 3, ch);
    if (memcmp(ch, "/*$", 3) == 0)
    {
        long pos = ftell(t->input);
        template_eat(t, 3, NULL);
        char buffer[MAX_STR_LEN];
        template_skip_spaces(t);
        template_read_string(t, buffer);
        if (strcmp(buffer, "else") == 0)
        {
            t->b_skip = b_ok;
            template_skip_until_block_comment_end(t);
            template_block(t, dict);
        }
        else
        {
            fseek(t->input, pos, SEEK_SET);
        }
    }
    if (!template_try_read_end_mark(t))
    {
        assert(false);
    }
    t->b_skip = false;
}

static void template_putc(template_t* t, char ch)
{
    if (!t->b_skip)
    {
        fputc(ch, t->output);
        if (ch == '\n')
        {
            t->out_line_start = ftell(t->output);
        }
    }
}

static void template_string(template_t* t, template_dict_t const* dict)
{
    char buffer[MAX_STR_LEN];
    template_eat(t, 1, NULL);
    template_read_string(t, buffer);
    if (!t->b_skip)
    {
        value_t* value = template_dict_get(dict, buffer);
        assert(value != NULL);
        assert(value->type == template_value_string);
        assert(value->str);
        fputs(value->str, t->output);
    }
    char ch;
    template_eat(t, 1, &ch);
    assert(ch == '$');
}

static bool template_try_read_tag(template_t* t, char const* name)
{
    long pos = ftell(t->input);
    char ch[3];
    template_peek(t->input, 3, ch);
    if (ch[0] == '/' && ch[1] == '*' && ch[2] == '$')
    {
        template_eat(t, 3, NULL);
        template_skip_spaces(t);
        char buffer[MAX_STR_LEN];
        template_read_string(t, buffer);
        if (strcmp(name, buffer) == 0)
        {
            return true;
        }
        else
        {
            fseek(t->input, pos, SEEK_SET);
            return false;
        }
    }
    return false;
}

static void template_switch(template_t* t, template_dict_t const* dict, char const* var_name)
{
    bool b_matched = false;
    t->b_skip = true;
    value_t* var = template_dict_get(dict, var_name);
    assert(var->type == template_value_string);
    while (true)
    {
        char buffer[MAX_STR_LEN];
        template_skip_spaces_ln(t);
        if (template_try_read_tag(t, "case"))
        {
            template_skip_spaces(t);
            template_read_string(t, buffer);
            template_skip_until_block_comment_end(t);
            if (!b_matched)
            {
                if (strcmp(var->str, buffer) == 0)
                {
                    t->b_skip = false;
                    b_matched = true;
                }
            }
            template_block(t, dict);
            t->b_skip = true;
            continue;
        }
        else if (template_try_read_tag(t, "default"))
        {
            if (!b_matched)
            {
                t->b_skip = false;
            }
            else
            {
                t->b_skip = true;
            }
            template_skip_until_block_comment_end(t);
            template_block(t, dict);
            continue;
        }
        else if (template_try_read_end_mark(t))
        {
            break;
        }
        assert(false);
    }
    t->b_skip = false;
}

static void template_block(template_t* t, template_dict_t const* dict)
{
    while (true)
    {
        char ch[5];
        template_peek(t->input, sizeof(ch), ch);
        if (ch[0] == '\0')
        {
            break;
        }
        if (ch[0] == '$' && isalpha(ch[1]))
        {
            template_string(t, dict);
            continue;
        }
        if (memcmp(ch, "//$", 3) == 0)
        {
            template_eat(t, 3, NULL);
            template_skip_one_space(t);
            continue;
        }
        if (memcmp(ch, "/*$", 3) == 0)
        {
            if (ch[3] == '*' && ch[4] == '/')
            {
                break;
            }
            long pos = ftell(t->input);
            template_eat(t, 3, NULL);
            template_skip_spaces(t);
            char buffer[MAX_STR_LEN];
            template_read_string(t, buffer);
            if (strcmp(buffer, "remove") == 0)
            {
                template_skip_until_next_end_mark(t);
                continue;
            }
            if (strcmp(buffer, "if") == 0)
            {
                template_skip_spaces(t);
                template_read_string(t, buffer);
                template_skip_until_block_comment_end(t);
                template_if(t, dict, buffer);
                continue;
            }
            if (strcmp(buffer, "else") == 0 ||
                strcmp(buffer, "endif") == 0 ||
                strcmp(buffer, "case") == 0 ||
                strcmp(buffer, "default") == 0)
            {
                fseek(t->input, pos, SEEK_SET);
                break;
            }
            if (strcmp(buffer, "foreach") == 0)
            {
                template_skip_spaces(t);
                template_read_string(t, buffer);
                template_skip_until_block_comment_end(t);
                template_foreach(t, buffer);
                continue;
            }
            if (strcmp(buffer, "switch") == 0)
            {
                template_skip_spaces(t);
                template_read_string(t, buffer);
                template_skip_until_block_comment_end(t);
                template_switch(t, dict, buffer);
                continue;
            }
            assert(false && "unknown directive");
        }
        template_putc(t, ch[0]);
        template_eat(t, 1, NULL);
    }
}

static void template_foreach(template_t* t, char const* array_name)
{
    value_t* value = template_dict_get(&t->value.dict, array_name);
    assert(value && value->type == template_value_array);
    template_array_t array = value->array;
    long start = ftell(t->input);
    for (uint64_t i = 0; i != array_size(array.values); i++)
    {
        assert(array.values[i].type == template_value_dict);
        template_block(t, &array.values[i].dict);
        if (i != array_size(array.values) - 1)
        {
            fseek(t->input, start, SEEK_SET);
        }
    }
    template_skip_until_next_end_mark(t);
}

static void template_gen(template_t* t, FILE* input, FILE* output)
{
    t->input = input;
    t->output = output;
    template_skip_bom(t);
    template_block(t, &t->value.dict);
}

static void template_value_destroy(value_t* value, Allocator* allocator)
{
    if (value->type == template_value_array)
    {
        for (size_t i = 0; i != array_size(value->array.values); i++)
        {
            template_value_destroy(&value->array.values[i], allocator);
        }
        array_free(allocator, value->array.values);
    }
    else if (value->type == template_value_dict)
    {
        for (size_t i = 0; i != array_size(value->dict.key_value_pairs); i++)
        {
            template_value_destroy(&value->dict.key_value_pairs[i].value, allocator);
        }
        array_free(allocator, value->dict.key_value_pairs);
    }
}

static void template_destroy(template_t* t)
{
    template_value_destroy(&t->value, t->allocator);
    allocator_free(t->allocator, t);
}
