#pragma once

#include "cup/node.h"

typedef struct CCompileCmd CCompileCmd;
typedef struct ScanDepsCmd ScanDepsCmd;

struct ScanDepsCmd
{
    struct Node;
    CCompileCmd* compile_cmd;
    char* export_name;
    char** imports;
};

Node* scan_deps_cmd_create(CCompileCmd* compile_cmd);