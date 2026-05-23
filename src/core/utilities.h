#pragma once

#include <stdint.h>

typedef struct Allocator Allocator;

char const* utilities_split_cmd(Allocator* allocator, char const* cmd, char** out);
uint64_t utilities_compute_file_hash(char const* path);
char** utilities_copy_string_array(Allocator* allocator, char const** strings);