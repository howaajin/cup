#pragma once

#include "cup/node.h"

#include <stdint.h>

typedef struct EmbeddedFile EmbeddedFile;

struct EmbeddedFile
{
    char const* base64;
    size_t* size;
    char const* path;
    FileType type;
    size_t struct_bytes;
};

Node* create_gen_embedded_file_cmd(EmbeddedFile* embedded_file);