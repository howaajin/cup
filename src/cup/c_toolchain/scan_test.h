#pragma once

#include "cup/node.h"

typedef struct CCompileCmd CCompileCmd;

struct ScanTestCmd
{
    struct Node;
    CCompileCmd* compile_cmd;
    Node* src;
    char** entries;
};
