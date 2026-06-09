#include "cup/embedded_file.h"
#include "core/allocator.h"
#include "core/codecvt.h"
#include "core/macros.h"
#include "core/os.h"

#include <stdlib.h>
#include <string.h>

static int gen_embedded_files_cb(Node* node)
{
    struct EmbeddedFile* e = node->ctx;
    Node* output = FILE(e->path);
    FILE* file = os_fopen(output->path, "wb");
    if (!file)
    {
        printf("cannot open file: %s\n", output->path);
        return EXIT_FAILURE;
    }
    uint8_t* buf = allocator_malloc(allocator_temp(), *e->size);
    expect(buf, "allocation failed");
    int n = base64_decode(e->base64, strlen(e->base64), buf, *e->size);
    fwrite(buf, 1, n, file);
    fclose(file);
    return EXIT_SUCCESS;
}

Node* create_gen_embedded_file_cmd(EmbeddedFile* embedded_file)
{
    extern bool b_dll_mode;
    Node* cmd = CALLBACK_CMD(gen_embedded_files_cb, embedded_file);
    cmd_set_source_location(cmd, __FILE__, __LINE__);
    uint32_t type = node_make_file_type(embedded_file->type, 0);
    Node* output = get_or_add_node(type, fmt(embedded_file->path), embedded_file->struct_bytes);
    output->file_type = embedded_file->type;
    cmd_add_output(cmd, output);
    if (b_dll_mode)
    {
        Node* self = get_or_add_file_with_type(fmt("{self}"), FILE_TYPE_EXE);
        cmd_add_input(cmd, self);
    }
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", output));
    return cmd;
}