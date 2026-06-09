#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep

#if defined(__TINYC__)
#define NODISCARD
#define NODISCARD_MSG(msg)
#else
#define NODISCARD [[nodiscard]]
#define NODISCARD_MSG(msg) [[nodiscard(msg)]]
#endif

#define field_parent(type, ptr, field) (type*)((uint8_t*)(ptr) - offsetof(type, field))
#define stringify(token) #token
#define static_array_size(a) (sizeof(a) / sizeof((a)[0]))
#define warn(fmt, ...) fprintf(stderr, "warning: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define error(fmt, ...) fprintf(stderr, "error: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define expect(cond, fmt, ...)                  \
    do                                          \
    {                                           \
        if (!(cond)) fatal(fmt, ##__VA_ARGS__); \
    } while (0)
#define fatal(fmt, ...)                                                                \
    do                                                                                 \
    {                                                                                  \
        fprintf(stderr, "fatal: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(1);                                                                       \
    } while (0)
#define ENUM_CODE(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
