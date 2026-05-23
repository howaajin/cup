#pragma once

#include <stdarg.h>

typedef struct Allocator Allocator;

char* fmt_alloc_v(Allocator* allocator, char const* fmt_str, va_list* args);
char const* fmt(char const* fmt_str, ...);
char* fmt_alloc(Allocator* allocator, char const* fmt_str, ...);
