#include "core/allocator.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

void* allocator_virtual_alloc(void* base_address, size_t size)
{
    return VirtualAlloc(base_address, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void allocator_virtual_free(void* base_address, size_t size)
{
    if (VirtualFree(base_address, 0, MEM_RELEASE) == 0)
    {
        printf("VirtualFree failed with error code %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }
}