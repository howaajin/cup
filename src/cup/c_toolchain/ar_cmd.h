#pragma once

#include "cup/c_toolchain/c_toolchain.h"
#include "cup/node.h"

typedef struct ArCmd ArCmd;
typedef struct Set Set;

struct ArCmd
{
    struct Node;
    ToolchainType toolchain;
    Node* output;
    Set* set_inputs;
    Node** ar_inputs;
};

Node* ar_cmd_create(Node* output, char const* file, int line);
void ar_cmd_add_input(Node* node, Node* input);
void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain);
