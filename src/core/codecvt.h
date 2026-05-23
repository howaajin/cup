#pragma once

#include "core/allocator.h"
#include "core/array.h"

#include <limits.h>
#include <stdbool.h>
#include <wchar.h>

static inline int utf8_decode(uint8_t const* p, uint32_t* out)
{
    uint8_t a = *p;
    if (a < 0x80)
    {
        *out = a;
        return 1;
    }
    else if ((a & 0xE0) == 0xC0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80)
        {
            return 0;
        }
        *out = (a & 0x1F) << 6 | (b & 0x3F);
        return 2;
    }
    else if ((a & 0xF0) == 0xE0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80) return 0;
        uint8_t c = *(p + 2);
        if ((c & 0xC0) != 0x80) return 0;
        *out = ((a & 0xF) << 12) | ((b & 0x3F) << 6) | (c & 0x3F);
        return 3;
    }
    else if ((a & 0xF8) == 0xF0)
    {
        uint8_t b = *(p + 1);
        if ((b & 0xC0) != 0x80) return 0;
        uint8_t c = *(p + 2);
        if ((c & 0xC0) != 0x80) return 0;
        uint8_t d = *(p + 3);
        if ((d & 0xC0) != 0x80) return 0;
        *out = ((a & 0x07) << 18) | ((b & 0x3F) << 12) | ((c & 0x3F) << 6) | (d & 0x3F);
        return 4;
    }
    return 0;
}

static inline int utf8_encode(uint32_t u, char out[4])
{
    if (u <= 0x7F)
    {
        out[0] = (char)u;
        return 1;
    }
    else if (u <= 0x7FF)
    {
        out[0] = (char)(0xC0 | (u >> 6));
        out[1] = (char)(0x80 | (u & 0x3F));
        return 2;
    }
    else if (u <= 0xFFFF)
    {
        out[0] = (char)(0xE0 | (u >> 12));
        out[1] = (char)(0x80 | ((u >> 6) & 0x3F));
        out[2] = (char)(0x80 | (u & 0x3F));
        return 3;
    }
    else
    {
        out[0] = (char)(0xF0 | (u >> 18));
        out[1] = (char)(0x80 | ((u >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((u >> 6) & 0x3F));
        out[3] = (char)(0x80 | (u & 0x3F));
        return 4;
    }
}

static inline int unicode_to_wchars(uint32_t u, uint16_t out[2])
{
    if (u <= 0xFFFF)
    {
        out[0] = (uint16_t)u;
        return 1;
    }
    else
    {
        uint32_t cp_offset = u - 0x10000;
        out[0] = (uint16_t)(0xD800 | (cp_offset >> 10));
        out[1] = (uint16_t)(0xDC00 | (cp_offset & 0x03FF));
        return 2;
    }
}

static inline int wchars_to_unicode(wchar_t const* p, uint32_t* out)
{
    uint32_t u = *p;
    if (u >= 0xD800 && u <= 0xD8FF)
    {
        uint32_t high = u;
        uint32_t low = *(p + 1);
        if (low >= 0xDC00 && low <= 0xDFFF)
        {
            u = 0x10000 + (((high & 0x3FF) << 10) | (low & 0x03FF));
            *out = u;
            return 2;
        }
        else
        {
            return 0;
        }
    }
    else if (u >= 0xDC00 && u <= 0xDFFF)
    {
        return 0;
    }
    *out = u;
    return 1;
}

static inline wchar_t* utf8_to_wchars(Allocator* allocator, const char* mbs)
{
    wchar_t* result = NULL;
    uint8_t const* p = (uint8_t const*)mbs;
    uint32_t u;
    while (1)
    {
        int n = utf8_decode(p, &u);
        if (n == 0) goto Error;
        p += n;
        uint16_t wchars[2];
        n = unicode_to_wchars(u, wchars);
        array_push_v(allocator, result, wchars, n);
        if (n == 1 && wchars[0] == 0)
        {
            break;
        }
    }
    array_push(allocator, result, L'\0');
    array_pop(result);
    return result;
Error:
    if (result)
    {
        array_free(allocator, result);
    }
    return NULL;
}

static inline char* wchars_to_utf8(Allocator* allocator, wchar_t const* wchars)
{
    char* result = NULL;
    wchar_t const* p = wchars;
    while (1)
    {
        uint32_t u;
        int n = wchars_to_unicode(p, &u);
        if (n == 0) goto Error;
        p += n;
        char out[4];
        n = utf8_encode(u, out);
        array_push_v(allocator, result, out, n);
        if (out[0] == 0)
        {
            break;
        }
    }
    array_pop(result);
    return result;
Error:
    if (result)
    {
        array_free(allocator, result);
    }
    return NULL;
}

static inline bool utf16_is_surrogate(uint16_t code)
{
    return code >= 0xD800 && code <= 0xDFFF;
}

static inline int utf16_to_utf32(const uint16_t* code, uint32_t* out)
{
    if (utf16_is_surrogate(*code))
    {
        if ((code[0] & 0xFFFFFC00) != 0xD800) return 0;
        if ((code[1] & 0xFFFFFC00) != 0xDC00) return 0;
        *out = ((uint32_t)code[0] << 10) + code[1] - 0x35FDC00;
        return 2;
    }
    *out = (uint32_t)*code;
    return 1;
}
