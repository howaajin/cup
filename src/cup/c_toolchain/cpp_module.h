#pragma once

#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"

typedef struct Node Node;

Node* module_from_src(Node* src);
Node* module_from_src_with_variant(Node* src, char const* variant);
Node* get_or_create_std_module_for_compile_cmd(CCompileCmd* cmd);
Node* get_or_create_std_compat_module_for_compile_cmd(CCompileCmd* cmd);
