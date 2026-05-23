#pragma once

#include "core/allocator.h"
#include "core/array.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define string_npos SIZE_MAX

#if defined(__GNUC__) || defined(__clang__)
#define STRING_FORMAT_CHECK(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define STRING_FORMAT_CHECK(fmt_idx, arg_idx)
#endif

[[nodiscard]]
static inline char* string_new(Allocator* allocator, size_t length, char const* data)
{
    char* str = array_new(allocator, char, length, length + 1);
    if (data)
    {
        memcpy(str, data, length);
    }
    str[length] = '\0';
    return str;
}

[[nodiscard]]
static inline char* string_clone(Allocator* allocator, char const* other)
{
    return string_new(allocator, array_size(other), other);
}

static inline void string_clear(char* str)
{
    if (array_size(str) == 0)
    {
        return;
    }
    array_resize(NULL, str, 0);
    str[0] = '\0';
}

static inline void string_free(Allocator* allocator, char* str)
{
    array_free(allocator, str);
}

[[nodiscard]]
static inline char* string_from_c_str(Allocator* allocator, char const* c_str)
{
    size_t len = c_str ? strlen(c_str) : 0;
    return string_new(allocator, len, c_str);
}

[[nodiscard]] STRING_FORMAT_CHECK(2, 0)
static inline char* string_from_vprint(Allocator* allocator, char const* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int num = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (num < 0) return NULL;
    char* result = string_new(allocator, (size_t)num, NULL);
    vsnprintf(result, num + 1, fmt, args);
    return result;
}

[[nodiscard]] STRING_FORMAT_CHECK(2, 3)
static inline char* string_from_print(Allocator* allocator, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* result = string_from_vprint(allocator, fmt, args);
    va_end(args);
    return result;
}

[[nodiscard("string_push may reallocate memory")]]
static inline char* string_push(Allocator* allocator, char* str, char ch)
{
    array_push(allocator, str, ch);
    array_push(allocator, str, '\0');
    array_size_lvalue(str) -= 1;
    return str;
}

[[nodiscard]]
static inline size_t string_length(char const* str)
{
    return array_size(str);
}

static inline char* string_ensure_space(Allocator* allocator, char* str, size_t old_len, size_t add_len)
{
    size_t old_capacity = array_capacity(str);
    assert((old_len == 0 && old_capacity == 0) || old_len < old_capacity);
    size_t new_size = old_len + add_len;
    if (old_capacity < new_size + 1)
    {
        char* new_str = string_new(allocator, new_size, NULL);
        if (old_len != 0)
        {
            memcpy(new_str, str, old_len);
        }
        array_free(allocator, str);
        return new_str;
    }
    else
    {
        array_size_lvalue(str) = new_size;
        return str;
    }
}

[[nodiscard("string_append_slice may reallocate memory")]]
static inline char* string_append_slice(Allocator* allocator, char* str1, size_t slice_len, char const* data)
{
    if (slice_len == 0)
    {
        return str1;
    }
    size_t str_len = string_length(str1);
    bool is_same_addr = (data >= str1 && data < str1 + str_len);
    char* new_str1 = string_ensure_space(allocator, str1, str_len, slice_len);
    if (is_same_addr)
    {
        data = new_str1 + (data - str1);
    }
    memcpy(new_str1 + str_len, data, slice_len);
    new_str1[str_len + slice_len] = '\0';
    return new_str1;
}

[[nodiscard("string_append_impl may reallocate memory")]]
static inline char* string_append_impl(Allocator* allocator, char* str1, char const* str2)
{
    return string_append_slice(allocator, str1, string_length(str2), str2);
}

[[nodiscard("string_append_c_str may reallocate memory")]]
static inline char* string_append_c_str(Allocator* allocator, char* str1, char const* str2)
{
    size_t str2_len = str2 ? strlen(str2) : 0;
    return string_append_slice(allocator, str1, str2_len, str2);
}

[[nodiscard("string_append_fmt may reallocate memory")]] STRING_FORMAT_CHECK(3, 4)
static inline char* string_append_fmt(Allocator* allocator, char* str, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* temp = string_from_vprint(allocator, fmt, args);
    va_end(args);
    if (!temp) return str;
    str = string_append_impl(allocator, str, temp);
    string_free(allocator, temp);
    return str;
}

[[nodiscard]]
static inline bool string_equal(char const* str1, char const* str2)
{
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    if (len1 != len2) return false;
    if (len1 == 0) return true;
    return memcmp(str1, str2, len1) == 0;
}

[[nodiscard("string_shrink_to_fit may reallocate memory")]]
static inline char* string_shrink_to_fit(Allocator* allocator, char* str)
{
    array_reserve(allocator, str, string_length(str) + 1);
    return str;
}

[[nodiscard]]
static inline size_t string_find_substr(char* str, size_t length, char const* data)
{
    if (length == 0) return string_npos;

    size_t str_len = string_length(str);
    if (str_len < length) return string_npos;

    char const* current = str;
    char const* end = str + str_len - length + 1;
    while ((current = (char const*)memchr(current, data[0], end - current)) != NULL)
    {
        if (memcmp(current, data, length) == 0)
        {
            return (size_t)(current - str);
        }
        current++;
    }

    return string_npos;
}

static inline bool string_starts_with(char const* str, char const* prefix)
{
    size_t len_prefix = strlen(prefix);
    return !strncmp(str, prefix, len_prefix);
}

static inline bool string_ends_with(char const* str, char const* suffix)
{
    if (!str || !suffix)
    {
        return false;
    }
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str)
    {
        return false;
    }
    return !strncmp(str + len_str - len_suffix, suffix, len_suffix);
}

static inline void string_toupper(char* str)
{
    while (*str)
    {
        *str = toupper(*str);
        ++str;
    }
}

static inline void string_tolower(char* str)
{
    while (*str)
    {
        *str = tolower(*str);
        ++str;
    }
}

static inline bool string_contains(char const* str, char const* sub_str)
{
    if (str == NULL && sub_str == NULL)
    {
        return true;
    }
    if (str == NULL || sub_str == NULL)
    {
        return false;
    }
    return strstr(str, sub_str);
}

#define string_printf(allocator, s, fmt, ...) ((s) = string_append_fmt(allocator, s, fmt, ##__VA_ARGS__))
#define string_putc(allocator, s, ch) ((s) = string_push(allocator, s, ch))
#define string_concat_c_str(allocator, s, c_str) ((s) = string_append_c_str(allocator, s, c_str))
#define string_append(allocator, s1, s2) ((s1) = string_append_impl(allocator, s1, s2))