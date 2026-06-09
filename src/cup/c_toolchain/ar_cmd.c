#include "cup/c_toolchain/ar_cmd.h"

#include "core/allocator.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/platform.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/ext_node_type.h"

#include <assert.h>

extern Allocator* node_allocator;
extern ArchitectureType default_architecture_type;
extern ToolchainType default_toolchain;

static char const* get_ar_name()
{
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC) return "lib";
    if (default_toolchain == TOOLCHAIN_TYPE_LLVM)
    {
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
        return "llvm-lib";
#elif CURRENT_PLATFORM == PLATFORM_MACOS
        return "ar rcs";
#else
        return "llvm-ar rcs";
#endif
    }
    if (default_toolchain == TOOLCHAIN_TYPE_GCC) return "ar rcs";
    if (default_toolchain == TOOLCHAIN_TYPE_ZIG) return "zig ar rcs";
    fatal("unknown toolchain");
    return NULL;
}

static char const* get_ar_option_out()
{
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC ||
        (default_toolchain == TOOLCHAIN_TYPE_LLVM && CURRENT_PLATFORM == PLATFORM_WINDOWS))
    {
        return "/out:";
    }
    else return "";
}

static void ar_cmd_prepare(Node* node)
{
    ArCmd* cmd = (ArCmd*)node;
    Node* env = get_toolchain_env_node(cmd->toolchain, default_architecture_type);
    cmd_set_env(node, env);
    char const* ar_name = get_ar_name();
    char const* opt_out = get_ar_option_out();
    cmd_add_option(node, OPTION_EXE, ar_name);
    if (opt_out[0] != '\0')
    {
        cmd_add_option(node, OPTION_FLAG, opt_out);
        cmd_add_option_no_sep(node, OPTION_OUTPUT, cmd->output->path);
    }
    else
    {
        cmd_add_option(node, OPTION_OUTPUT, cmd->output->path);
    }
    if (default_toolchain == TOOLCHAIN_TYPE_MSVC)
    {
        cmd_add_option(node, OPTION_FLAG, "/nologo");
    }
    for (size_t i = 0; i != array_size(cmd->ar_inputs); i++)
    {
        Node* input = cmd->ar_inputs[i];
        if (input->file_type != FILE_TYPE_OBJ)
        {
            continue;
        }
        cmd_add_option(node, OPTION_INPUT, input->path);
        cmd_add_input(node, input);
    }
    cmd_prepare(node);
}

Node* ar_cmd_create(Node* output, char const* file, int line)
{
    expect(output->build_cmd == NULL, "output already has a build command");
    uint32_t node_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_AR);
    Node* cmd = node_create(node_type, NULL, sizeof(ArCmd));
    ArCmd* ar_cmd = (ArCmd*)cmd;
    ar_cmd->toolchain = default_toolchain;
    ar_cmd->output = output;
    ar_cmd->set_inputs = allocator_calloc(node_allocator, 1, sizeof(Set));
    ar_cmd->set_inputs->allocator = node_allocator;
    ar_cmd->prepare = ar_cmd_prepare;
    cmd_set_source_location(cmd, file, line);
    cmd_add_output(cmd, output);
    return cmd;
}

void ar_cmd_add_input(Node* node, Node* input)
{
    if (input->node_type != NODE_TYPE_FILE || (input->file_type != FILE_TYPE_OBJ && input->file_type != FILE_TYPE_LIB))
    {
        warn("An ar cmd input can only be a .lib (library) or another .obj (object file).");
        return;
    }
    ArCmd* cmd = (ArCmd*)node;
    array_push(node_allocator, cmd->ar_inputs, input);
}

void ar_cmd_set_toolchain_type(Node* node, ToolchainType toolchain)
{
    ArCmd* cmd = (ArCmd*)node;
    cmd->toolchain = toolchain;
}
