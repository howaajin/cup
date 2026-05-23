#include "core/json.h"
#include "core/array.h"
#include "core/codecvt.h"
#include "core/hash.h"
#include "core/string.h"

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
