#include "cup/entry.h"
#include "core/allocator.h"
#include "core/array.h"

static Entry* entries = NULL;

Entry* entry_get_all(void)
{
    return entries;
}

void entry_push(Entry entry)
{
    array_push(allocator_c(), entries, entry);
}

void entry_clean()
{
    array_free(allocator_c(), entries);
}