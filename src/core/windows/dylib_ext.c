#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdint.h>

#include "core/dylib.h"
#include "core/string.h"

char** dylib_list_symbols(Dylib* lib, Allocator* allocator)
{
    uintptr_t base = (uintptr_t)lib;
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)base;
    uintptr_t nt_header_addr = dos_header->e_lfanew;
    IMAGE_NT_HEADERS* nt_header = (IMAGE_NT_HEADERS*)(base + nt_header_addr);

    DWORD export_table_rva = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(base + export_table_rva);
    char** result = NULL;
    if (exports->AddressOfNames)
    {
        DWORD* names_rva = (DWORD*)(base + exports->AddressOfNames);
        for (uint64_t i = 0; i < exports->NumberOfNames; i++)
        {
            char const* name = (char const*)(base + names_rva[i]);
            char* symbol = string_from_c_str(allocator, name);
            array_push(allocator, result, symbol);
        }
    }
    return result;
}
