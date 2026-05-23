#pragma once

#include "cup/c_toolchain/c_toolchain.h"
#include "cup/node.h"

typedef struct Node Node;
typedef struct LinkCmd LinkCmd;

struct LinkCmd
{
    struct Node;
    Node* output;
    Node* pdb;
    Node* def;
    Node* out_import_lib;
    Node** input_option_files;
    char const** libs;
    char const** lib_directories;
    char const** flags;
    char* entry;
    ToolchainType toolchain;
    LinkerType linker_type;
    OptimizationType optimization;
    ArchitectureType arch;
    bool b_generate_debug_info;
    bool b_link_cpp;
};

