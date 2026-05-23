#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void* allocator_virtual_alloc(void* base_address, size_t size)
{
    void* ptr = mmap(base_address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void allocator_virtual_free(void* base_address, size_t size)
{
    if (munmap(base_address, size) == -1)
    {
        perror("munmap failed");
        exit(EXIT_FAILURE);
    }
}