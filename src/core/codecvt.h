#pragma once

#include "core/allocator.h"
#include "core/array.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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

static inline size_t base64_encode_size(size_t data_len)
{
    return ((data_len + 2) / 3) * 4 + 1;
}

static inline void base64_encode(char* out, uint8_t const* data, size_t data_len)
{
    // clang-format off
    static char const table[64] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
    };
    // clang-format on

    size_t fast_len = data_len - (data_len % 3);
    for (size_t i = 0; i < fast_len; i += 3)
    {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = table[(v >> 6) & 0x3F];
        *out++ = table[v & 0x3F];
    }

    size_t rem = data_len - fast_len;
    if (rem == 1)
    {
        uint32_t v = (uint32_t)data[fast_len] << 16;
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = '=';
        *out++ = '=';
    }
    else if (rem == 2)
    {
        uint32_t v = ((uint32_t)data[fast_len] << 16) | ((uint32_t)data[fast_len + 1] << 8);
        *out++ = table[(v >> 18) & 0x3F];
        *out++ = table[(v >> 12) & 0x3F];
        *out++ = table[(v >> 6) & 0x3F];
        *out++ = '=';
    }
    *out = '\0';
}

static inline int base64_decode(char const* in, size_t in_len, uint8_t* out, size_t out_cap)
{
    // clang-format off
    static uint8_t const decode_table[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,62, 255,255,255,63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255,255,255,255,255,255,
        255,0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255,255,255,255,255,
        255,26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255,255,255,255,255,
    };
    // clang-format on

    while (in_len > 0 && in[in_len - 1] == '=')
    {
        in_len--;
    }
    if (in_len == 0) return 0;

    size_t rem = in_len % 4;
    if (rem == 1) return -1;

    size_t out_len = (in_len / 4) * 3 + (rem == 2 ? 1 : (rem == 3 ? 2 : 0));
    if (out_len > out_cap) return -1;

    size_t fast_iters = in_len / 4;
    size_t in_pos = 0;
    size_t pos = 0;

    for (size_t i = 0; i < fast_iters; i++)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c2 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c3 = decode_table[(uint8_t)in[in_pos++]];

        if ((c0 | c1 | c2 | c3) & 0xC0) return -1;

        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        out[pos++] = (uint8_t)((c1 << 4) | (c2 >> 2));
        out[pos++] = (uint8_t)((c2 << 6) | c3);
    }

    if (rem == 2)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos]];
        if ((c0 | c1) & 0xC0) return -1;
        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
    }
    else if (rem == 3)
    {
        uint8_t c0 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c1 = decode_table[(uint8_t)in[in_pos++]];
        uint8_t c2 = decode_table[(uint8_t)in[in_pos]];
        if ((c0 | c1 | c2) & 0xC0) return -1;
        out[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        out[pos++] = (uint8_t)((c1 << 4) | (c2 >> 2));
    }

    return (int)out_len;
}