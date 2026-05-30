#pragma once

#include <stdarg.h>

typedef struct Allocator Allocator;

/// Format a string with curly-brace placeholders and variadic arguments.
///
/// Placeholders:
///   `{}`         — auto-indexed, consumes next variadic arg as string (const char*)
///   `{:s}`       — same as `{}`, explicitly typed string
///   `{:n}`       — arg is a Node*, outputs `node->name`
///   `{:d}`       — arg is an int, outputs decimal
///   `{0}` `{1}`  — positional index, type defaults to string
///   `{name}`     — named variable, resolved via get_var(); no variadic arg consumed
char const* fmt(char const* fmt_str, ...);
char* fmt_alloc_v(Allocator* allocator, char const* fmt_str, va_list* args);
char* fmt_alloc(Allocator* allocator, char const* fmt_str, ...);