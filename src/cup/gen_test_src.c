#include "core/macros.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/embedded_file.h"

#include <stdint.h>
#include <stdlib.h>

extern size_t bin2c_test_h_size;
extern char const bin2c_test_h[];
extern size_t bin2c_test_c_size;
extern char const bin2c_test_c[];
extern size_t bin2c_test_main_c_size;
extern char const bin2c_test_main_c[];

static struct EmbeddedFile embedded_files[] = {
    {
        .base64 = bin2c_test_h,
        .size = &bin2c_test_h_size,
        .path = "{out_dir}/cup/test.h",
        .type = FILE_TYPE_NORMAL,
        .struct_bytes = sizeof(Node),
    },
    {
        .base64 = bin2c_test_c,
        .size = &bin2c_test_c_size,
        .path = "{out_dir}/cup/test.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    },
    {
        .base64 = bin2c_test_main_c,
        .size = &bin2c_test_main_c_size,
        .path = "{out_dir}/cup/test_main.c",
        .type = FILE_TYPE_SRC,
        .struct_bytes = sizeof(Src),
    },
};

void gen_test_src(void)
{
    for (size_t i = 0; i != static_array_size(embedded_files); i++)
    {
        create_gen_embedded_file_cmd(&embedded_files[i]);
    }
}