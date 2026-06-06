#include "core/allocator.h"
#include "core/path.h"

#include <crt_externs.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char const* os_get_cmdline(void)
{
    static char cmdline[65536] = {0};

    if (cmdline[0] != '\0')
    {
        return cmdline;
    }

    int argc = *_NSGetArgc();
    char** argv = *_NSGetArgv();

    size_t offset = 0;
    for (int i = 0; i < argc; i++)
    {
        size_t len = strlen(argv[i]);
        if (offset + len + 1 >= sizeof(cmdline))
        {
            break;
        }
        memcpy(cmdline + offset, argv[i], len);
        offset += len;
        cmdline[offset++] = ' ';
    }

    if (offset > 0)
    {
        cmdline[offset - 1] = '\0';
    }

    return cmdline;
}

char* get_absolute_path(char const* path, Allocator* allocator);

char* os_get_current_exe_path(Allocator* allocator)
{
    char pathbuf[PATH_MAX];
    uint32_t size = sizeof(pathbuf);

    if (_NSGetExecutablePath(pathbuf, &size) == 0)
    {
        Allocator* stack_allocator = allocator_temp();
        char const* path = get_absolute_path(pathbuf, stack_allocator);
        return path_lexically_normal(path, allocator);
    }

    return NULL;
}

uint64_t os_get_rand_uint64()
{
    uint64_t r;
    arc4random_buf(&r, sizeof(r));
    return r;
}
