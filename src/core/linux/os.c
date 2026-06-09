#include "core/os.h"
#include "core/allocator.h"
#include "core/macros.h"
#include "core/path.h"
#include "core/utilities.h"


#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char const* os_get_cmdline(void)
{
    FILE* file = fopen("/proc/self/cmdline", "r");
    if (!file)
    {
        perror("fopen");
        return NULL;
    }
    static char cmdline[65536];
    size_t bytesRead = fread(cmdline, 1, sizeof(cmdline) - 1, file);
    fclose(file);
    for (size_t i = 0; i < bytesRead; i++)
    {
        if (cmdline[i] == '\0')
        {
            cmdline[i] = ' ';
        }
    }
    return cmdline;
}

char* get_absolute_path(char const* path, Allocator* allocator);

char* os_get_current_exe_path(Allocator* allocator)
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    expect(len != -1, "Failed to read /proc/self/exe");
    buffer[len] = '\0';
    char* result = path_lexically_normal(buffer, allocator);
    expect(result, "result is NULL");
    return result;
}

uint64_t os_get_rand_uint64()
{
    uint64_t r;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        perror("open /dev/urandom");
        return 0;
    }
    if (read(fd, &r, sizeof(r)) != sizeof(r))
    {
        perror("read");
        close(fd);
        return 0;
    }
    close(fd);
    return r;
}
