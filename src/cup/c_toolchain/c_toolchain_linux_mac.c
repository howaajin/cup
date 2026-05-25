#include "core/macros.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/node.h"

#include <assert.h>
#include <stdlib.h>

Node* msvc_get_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return NULL;
}

char const* msvc_find_std_module_source(bool b_compat)
{
    return NULL;
}

Node* get_toolchain_env_node(ToolchainType toolchain_type, ArchitectureType arch)
{
    return NULL;
}

ToolchainType c_toolchain_select_toolchain_automatically()
{
    ToolchainType toolchain = get_toolchain_by_current_compiler();
    bool b_no_llvm = false;
    bool b_no_gcc = false;
    if (toolchain == TOOLCHAIN_TYPE_LLVM)
    {
        if (system("clang --version > /dev/null 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_LLVM;
        }
        else
        {
            b_no_llvm = true;
        }
    }
    if (toolchain == TOOLCHAIN_TYPE_GCC)
    {
        if (system("gcc -v > /dev/null 2>&1") == 0)
        {
            return TOOLCHAIN_TYPE_GCC;
        }
        else
        {
            b_no_gcc = true;
        }
    }
    if (!b_no_llvm && system("clang --version > /dev/null 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_LLVM;
    }
    if (!b_no_gcc && system("gcc -v > /dev/null 2>&1") == 0)
    {
        return TOOLCHAIN_TYPE_GCC;
    }
    error("Cannot find the C compiler");
    exit(EXIT_FAILURE);
}
