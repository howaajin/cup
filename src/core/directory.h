#pragma once

#include <stdbool.h>

typedef struct Directory Directory;
typedef struct Allocator Allocator;

typedef struct DirectoryEntry
{
    bool is_directory;
    char* name;
} DirectoryEntry;

Directory* directory_open(const char* path, Allocator* allocator);
DirectoryEntry* directory_read(Directory* dir);
void directory_close(Directory* dir);
