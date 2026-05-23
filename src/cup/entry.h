#pragma once

#include "core/platform.h"

typedef void FnEntry(void);

typedef struct Entry
{
    char const* name;
    FnEntry* fn;
    char const* file;
    int line;
    int priority;
} Entry;

static inline int entry_compare(void const* a, void const* b)
{
    Entry* entry_a = (Entry*)a;
    Entry* entry_b = (Entry*)b;
    return entry_a->priority - entry_b->priority;
}

Entry* entry_get_all(void);
void entry_push(Entry entry);
void entry_clean();

#define PRIORITY_BEFORE_DEFAULT 50
#define PRIORITY_DEFAULT 100
#define PRIORITY_AFTER_DEFAULT 200
#define PRIORITY_AFTER_PREPARE 300

#define ENTRY(fn, ...)                                  \
    CONSTRUCTOR(Entry_register_##fn)                    \
    void Entry_register_##fn(void)                      \
    {                                                   \
        void fn(void);                                  \
        Entry e = {#fn, fn, __FILE__, __LINE__, 0};     \
        e.priority = (PRIORITY_DEFAULT, ##__VA_ARGS__); \
        entry_push(e);                                  \
    }                                                   \
    void fn(void)
