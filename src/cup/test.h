#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef void FnTestEntry();

typedef struct TestEntry
{
    char const* name;
    char const* group;
    FnTestEntry* fn;
    char const* file;
    int line;
} TestEntry;

#ifdef BUILD_TEST
#include <stdlib.h>
#ifdef __cplusplus
extern "C" TestEntry* test_entries;
extern "C" int num_test_entries;
#else
extern TestEntry* test_entries;
extern int num_test_entries;
#endif
static inline void test_push_entry(TestEntry entry)
{
    test_entries = (TestEntry*)realloc(test_entries, (num_test_entries + 1) * sizeof(TestEntry));
    test_entries[num_test_entries++] = entry;
}
#ifndef CONSTRUCTOR
#if defined(_MSC_VER)
#define CONSTRUCTOR(name)                   \
    _Pragma("section(\".CRT$XCU\", read)"); \
    static void name(void);                 \
    __declspec(allocate(".CRT$XCU")) void (*name##_ptr)(void) = name;
#else
#define CONSTRUCTOR(name) __attribute__((constructor(101)))
#endif
#endif
#define TEST_REGISTER(fn, ...)                                     \
    extern void fn(void);                                          \
    CONSTRUCTOR(fn##_register)                                     \
    static void fn##_register()                                    \
    {                                                              \
        TestEntry e = {#fn, #__VA_ARGS__, fn, __FILE__, __LINE__}; \
        test_push_entry(e);                                        \
    }
#else
#define TEST_REGISTER(fn, ...)
#endif // BUILD_TEST

#define TEST(fn, ...) TEST_REGISTER(fn, ##__VA_ARGS__) void fn(void)
#define TEST_DISABLED(fn, ...) void fn(void)

static inline void assert_impl(bool cond, char const* msg, char const* file, int line)
{
    if (!cond)
    {
        fprintf(stderr, "  ASSERT FAILED: %s:%d: %s\n", file, line, msg);
        exit(EXIT_FAILURE);
    }
}

#define ASSERT(cond) assert_impl(cond, #cond, __FILE__, __LINE__)