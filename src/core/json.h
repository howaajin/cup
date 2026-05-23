#pragma once

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