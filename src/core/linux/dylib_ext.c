#include "core/allocator.h"
#include "core/dylib.h"
#include "core/string.h"

#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>

typedef struct Dylib
{
    char const* name;
    void* handle;
} Dylib;

char** dylib_list_symbols(Dylib* lib, Allocator* allocator)
{
    char** result = NULL;
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        fprintf(stderr, "ELF library initialization failed.\n");
        return result;
    }
    int fd = open(lib->name, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return result;
    }

    Elf* elf = elf_begin(fd, ELF_C_READ, NULL);
    if (!elf)
    {
        fprintf(stderr, "elf_begin() failed.\n");
        close(fd);
        return result;
    }

    Elf_Scn* scn = NULL;
    GElf_Shdr shdr;

    while ((scn = elf_nextscn(elf, scn)) != NULL)
    {
        if (gelf_getshdr(scn, &shdr) != &shdr)
        {
            fprintf(stderr, "gelf_getshdr() failed.\n");
            continue;
        }

        if (shdr.sh_type == SHT_DYNSYM)
        {
            Elf_Data* data = elf_getdata(scn, NULL);
            if (!data)
            {
                fprintf(stderr, "elf_getdata() failed.\n");
                continue;
            }

            size_t num_symbols = shdr.sh_size / shdr.sh_entsize;

            for (size_t i = 0; i < num_symbols; ++i)
            {
                GElf_Sym sym;
                if (!gelf_getsym(data, i, &sym))
                {
                    fprintf(stderr, "gelf_getsym() failed.\n");
                    continue;
                }
                const char* name = elf_strptr(elf, shdr.sh_link, sym.st_name);
                if (name && GELF_ST_TYPE(sym.st_info) == STT_FUNC && (GELF_ST_BIND(sym.st_info) == STB_GLOBAL) && GELF_ST_VISIBILITY(sym.st_other) == STV_DEFAULT)
                {
                    array_push(allocator, result, string_from_c_str(allocator, name));
                }
            }
        }
    }

    elf_end(elf);
    close(fd);
    return result;
}