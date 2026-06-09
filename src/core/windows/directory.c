#include "core/directory.h"
#include "core/allocator.h"
#include "core/codecvt.h"
#include "core/path.h"
#include "core/string.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define FIND_PATH_POSTFIX "\\*"

typedef struct Directory
{
    Allocator* allocator;
    HANDLE handle;
    WIN32_FIND_DATAW find_data;
    DirectoryEntry current_entry;
    char* path;
} Directory;

Directory* directory_open(const char* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_temp();
    size_t path_len = strlen(path);
    size_t buffer_size = path_len + sizeof(FIND_PATH_POSTFIX);
    char* buffer = NULL;
    array_resize(temp_allocator, buffer, buffer_size);
    strcpy(buffer, path);
    strcat(buffer, FIND_PATH_POSTFIX);
    wchar_t* wpath = utf8_to_wchars(temp_allocator, buffer);

    Directory* dir = allocator_malloc(allocator, sizeof(Directory));
    expect(dir, "allocation failed");
    dir->allocator = allocator;
    dir->handle = FindFirstFileW(wpath, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        allocator_free(allocator, dir);
        return NULL;
    }
    else
    {
        dir->path = string_from_c_str(allocator, path);
        dir->current_entry.name = string_from_c_str(allocator, "");
        return dir;
    }
}

DirectoryEntry* directory_read(Directory* dir)
{
    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }
    else
    {
        dir->current_entry.is_directory = (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        array_resize(dir->allocator, dir->current_entry.name, 0);
        Allocator* stack_allocator = allocator_temp();
        char const* name = wchars_to_utf8(stack_allocator, dir->find_data.cFileName);
        string_append(dir->allocator, dir->current_entry.name, name);
        if (!FindNextFileW(dir->handle, &dir->find_data))
        {
            FindClose(dir->handle);
            dir->handle = INVALID_HANDLE_VALUE;
        }
        return &dir->current_entry;
    }
}

void directory_close(Directory* dir)
{
    array_free(dir->allocator, dir->current_entry.name);
    array_free(dir->allocator, dir->path);
    if (dir->handle != INVALID_HANDLE_VALUE)
    {
        FindClose(dir->handle);
    }
    allocator_free(dir->allocator, dir);
}