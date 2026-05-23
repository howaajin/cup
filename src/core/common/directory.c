#include "core/directory.h"
#include "core/allocator.h"
#include "core/path.h"
#include "core/string.h"

#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>

typedef struct Directory
{
    Allocator* allocator;
    DIR* dir;
    DirectoryEntry current_entry;
    char* path;
} Directory;

Directory* directory_open(const char* path, Allocator* allocator)
{
    DIR* d = opendir(path);
    if (d)
    {
        Directory* directory = allocator_malloc(allocator, sizeof(Directory));
        directory->allocator = allocator;
        directory->dir = d;
        directory->current_entry.name = string_from_c_str(allocator, "");
        directory->path = string_from_c_str(allocator, path);
        return directory;
    }
    else
    {
        return NULL;
    }
}

DirectoryEntry* directory_read(Directory* dir)
{
    struct dirent* entry = readdir(dir->dir);
    if (entry != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            dir->current_entry.is_directory = true;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            char const* full_path = path_combine(allocator_temp(), dir->path, entry->d_name, NULL);
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                dir->current_entry.is_directory = S_ISDIR(st.st_mode);
            }
            else
            {
                dir->current_entry.is_directory = false;
            }
        }
        else
        {
            dir->current_entry.is_directory = false;
        }
        array_resize(dir->allocator, dir->current_entry.name, 0);
        string_printf(dir->allocator, dir->current_entry.name, "%s", entry->d_name);
        return &dir->current_entry;
    }
    else
    {
        return NULL;
    }
}

void directory_close(Directory* dir)
{
    array_free(dir->allocator, dir->current_entry.name);
    array_free(dir->allocator, dir->path);
    closedir(dir->dir);
    allocator_free(dir->allocator, dir);
}