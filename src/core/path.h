#pragma once

#include <stdbool.h>

typedef struct Allocator Allocator;
typedef struct PathParser PathParser;

typedef enum PathParseStatus
{
    PARSE_STATUS_BEGIN = 0,
    PARSE_STATUS_AFTER_ROOT_NAME,
    PARSE_STATUS_AFTER_ROOT_DIRECTORY,
} PathParseStatus;

char* path_lexically_relative(char const* path, char const* base, Allocator* allocator);
char* path_lexically_normal(char const* path, Allocator* allocator);
char* path_root_path(char const* path, Allocator* allocator);
char* path_root_name(char const* path, Allocator* allocator);
char* path_root_directory(char const* path, Allocator* allocator);
char const* path_relative_path(char const* path);
char* path_parent_path(char const* path, Allocator* allocator);
char* path_filename(char const* path, Allocator* allocator);
char* path_stem(char const* path, Allocator* allocator);
char const* path_extension(char const* path);
char* path_replace_extension(char const* path, char const* ext, Allocator* allocator);
void* path_backslash_to_slash(char* path);
void path_slash_to_backslash(char* path);
char* path_combine(Allocator* allocator, char const* path, ... /* last must be NULL */);

bool path_is_absolute(char const* path);
bool path_is_empty(char const* path);
bool path_is_under_directory(char const* path, char const* directory);
bool path_has_relative_path(char const* path);
char* path_windows_style_to_linux_relative(char const* path, Allocator* allocator);

PathParser* path_create_parser(char const* path, Allocator* allocator);
PathParser* path_create_parser_with_status(char const* path, PathParseStatus status, Allocator* allocator);
char* path_next_element(PathParser* parser);
PathParseStatus path_get_parse_status(PathParser* parser);
