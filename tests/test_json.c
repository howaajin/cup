#include "core/allocator.h"
#include "core/array.h"
#include "core/json.h"
#include "core/string.h"
#include "cup/test.h"

#include <math.h>

#define ASSERT_JSON_STRING(value, expected)                \
    do                                                     \
    {                                                      \
        ASSERT((value) != NULL);                           \
        ASSERT((value)->type == JSON_TYPE_STRING);         \
        ASSERT(string_equal((value)->string, (expected))); \
    } while (0)

TEST(test_json_object_array_values, json)
{
    Allocator* allocator = allocator_create_chained();
    JsonValue json = json_from_string(
        " { \"name\": \"cup\", \"count\": 3, \"enabled\": true, \"items\": [null, false, 2.5] } ",
        allocator);

    ASSERT(json.type == JSON_TYPE_OBJECT);

    JsonValue* name = json_object_get_value(&json.object, "name");
    ASSERT_JSON_STRING(name, "cup");

    JsonValue* count = json_object_get_value(&json.object, "count");
    ASSERT(count->type == JSON_TYPE_NUMBER);
    ASSERT(fabs(count->number - 3.0) < 0.000001);

    JsonValue* enabled = json_object_get_value(&json.object, "enabled");
    ASSERT(enabled->type == JSON_TYPE_TRUE);

    JsonValue* missing = json_object_get_value(&json.object, "missing");
    ASSERT(missing == NULL);

    JsonValue* items = json_object_get_value(&json.object, "items");
    ASSERT(items->type == JSON_TYPE_ARRAY);
    ASSERT(array_size(items->array) == 3);
    ASSERT(items->array[0].type == JSON_TYPE_NULL);
    ASSERT(items->array[1].type == JSON_TYPE_FALSE);
    ASSERT(items->array[2].type == JSON_TYPE_NUMBER);
    ASSERT(fabs(items->array[2].number - 2.5) < 0.000001);

    allocator_destroy(allocator);
}

TEST(test_json_string_escapes, json)
{
    Allocator* allocator = allocator_create_chained();
    JsonValue json = json_from_string("\"quote: \\\" slash: \\/ backslash: \\\\\"", allocator);

    ASSERT(json.type == JSON_TYPE_STRING);
    ASSERT(string_equal(json.string, "quote: \" slash: / backslash: \\"));

    json = json_from_string("\"line\\nreturn\\rtab\\tback\\bform\\f\"", allocator);
    ASSERT(json.type == JSON_TYPE_STRING);
    ASSERT(string_equal(json.string, "line\nreturn\rtab\tback\bform\f"));

    allocator_destroy(allocator);
}

TEST(test_json_unicode_escapes, json)
{
    Allocator* allocator = allocator_create_chained();

    JsonValue json = json_from_string("\"latin: \\u00E9\"", allocator);
    ASSERT(json.type == JSON_TYPE_STRING);
    ASSERT(string_equal(json.string, "latin: \xC3\xA9"));

    json = json_from_string("\"music: \\uD834\\uDD1E\"", allocator);
    ASSERT(json.type == JSON_TYPE_STRING);
    ASSERT(string_equal(json.string, "music: \xF0\x9D\x84\x9E"));

    allocator_destroy(allocator);
}

TEST(test_json_invalid_inputs, json)
{
    Allocator* allocator = allocator_create_chained();

    ASSERT(json_from_string("{\"a\":}", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("[1,]", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("{\"a\":1,}", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("\"bad\\xescape\"", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("\"bad\\u123\"", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("\"bad\\uD834\"", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("\"bad\ncontrol\"", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string(NULL, allocator).type == JSON_TYPE_INVALID);

    allocator_destroy(allocator);
}

TEST(test_json_rejects_trailing_tokens, json)
{
    Allocator* allocator = allocator_create_chained();

    ASSERT(json_from_string("{\"a\":1} trailing", allocator).type == JSON_TYPE_INVALID);
    ASSERT(json_from_string("true false", allocator).type == JSON_TYPE_INVALID);

    allocator_destroy(allocator);
}
