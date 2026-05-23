#include "core/os.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/embedded_file.h"
#include "cup/entry.h"

ENTRY(gen_build_c)
{
    extern size_t bin2c_build_script_tpl_c_size;
    extern uint8_t bin2c_build_script_tpl_c[];

    if (os_file_exists("build.c"))
    {
        return;
    }

    static struct EmbeddedFile file = {
        .src = bin2c_build_script_tpl_c,
        .size = &bin2c_build_script_tpl_c_size,
        .path = "build.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    };
    create_gen_embedded_file_cmd(&file);
}
