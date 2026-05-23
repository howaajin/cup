#pragma once

#include "core/directory.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/cache.h"
#include "cup/entry.h"
#include "cup/fmt.h"
#include "cup/graph.h"
#include "cup/node.h"
#include "cup/var.h"

typedef struct Node Node;

void set_vs_project_version(char const* version);
void set_vscode_debugger_type(char const* type);
void set_cup_h_dir(char const* dir);
void set_node_default_excluded(bool b_default_excluded);
Node* add_copy_cmd(Node* input, Node* output, char const* file, int line);

int execute(void);

#define CUP_VERSION "1.0.0"

#define COPY(src, dst) add_copy_cmd(src, dst, __FILE__, __LINE__)