#include "core/dylib.h"

#include <fcntl.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct Dylib
{
    char const* name;
    void* handle;
} Dylib;

static uint32_t swap_uint32(uint32_t val, bool is_swap)
{
    return is_swap ? __builtin_bswap32(val) : val;
}

char** dylib_list_symbols(Dylib* lib, Allocator* allocator)
{
    char** result = NULL;

    int fd = open(lib->name, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return result;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        close(fd);
        return result;
    }

    void* file_data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_data == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return result;
    }

    uint32_t magic = *(uint32_t*)file_data;
    uint32_t file_offset = 0;

    bool is_swap = (magic == FAT_CIGAM || magic == FAT_CIGAM_64);
    if (magic == FAT_MAGIC || magic == FAT_CIGAM)
    {
        struct fat_header* fat = (struct fat_header*)file_data;
        uint32_t nfat_arch = swap_uint32(fat->nfat_arch, is_swap);
        struct fat_arch* archs = (struct fat_arch*)(fat + 1);

#if defined(__aarch64__)
        cpu_type_t target_cpu = CPU_TYPE_ARM64;
#else
        cpu_type_t target_cpu = CPU_TYPE_X86_64;
#endif

        for (uint32_t i = 0; i < nfat_arch; i++)
        {
            if (swap_uint32(archs[i].cputype, is_swap) == target_cpu)
            {
                file_offset = swap_uint32(archs[i].offset, is_swap);
                break;
            }
        }
    }

    struct mach_header_64* header = (struct mach_header_64*)((uint8_t*)file_data + file_offset);

    if (header->magic != MH_MAGIC_64)
    {
        fprintf(stderr, "Not a valid 64-bit Mach-O binary or target architecture not found.\n");
        munmap(file_data, st.st_size);
        close(fd);
        return result;
    }

    struct load_command* lc = (struct load_command*)(header + 1);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (lc->cmd == LC_SYMTAB)
        {
            struct symtab_command* symtab = (struct symtab_command*)lc;

            struct nlist_64* syms = (struct nlist_64*)((uint8_t*)file_data + file_offset + symtab->symoff);
            char* strtab = (char*)file_data + file_offset + symtab->stroff;

            for (uint32_t j = 0; j < symtab->nsyms; j++)
            {
                if ((syms[j].n_type & N_EXT) && ((syms[j].n_type & N_TYPE) == N_SECT))
                {
                    char const* name = strtab + syms[j].n_un.n_strx;

                    if (name[0] == '_')
                    {
                        name++;
                    }

                    array_push(allocator, result, string_from_c_str(allocator, name));
                }
            }
            break;
        }
        lc = (struct load_command*)((uint8_t*)lc + lc->cmdsize);
    }

    munmap(file_data, st.st_size);
    close(fd);
    return result;
}
