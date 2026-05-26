#include "cup.h"

// set_after_prepare_callback(setup_default_flags);
// The `setup_default_flags` function will be called after the prepare phase, at which point
// the dependencies of all targets have been determined and no new targets will be generated.
void setup_default_flags(void)
{
    Node** nodes = get_all_nodes();
    size_t num_nodes = array_size(nodes);
    uint32_t compile_cmd_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE);
    uint32_t link_cmd_type = node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_LINK);
    for (size_t i = 0; i != num_nodes; i++)
    {
        Node* node = nodes[i];
        if (node->type == compile_cmd_type)
        {
            CCompileCmd* cmd = (CCompileCmd*)node;
            // c_compile_cmd_add_include_directory(node, "src");
            // c_compile_cmd_set_cpp_std(node, CPP_LANGUAGE_STANDARD_20);
            // c_compile_cmd_set_c_std(node, C_LANGUAGE_STANDARD_23);
            if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
            {
                // c_compile_cmd_add_flag(node, "-Wall");
                // c_compile_cmd_add_flag(node, "-Wextra");
            }
            // if (cmd->toolchain != TOOLCHAIN_TYPE_MSVC)
            // {
            //     c_compile_cmd_add_flag(node, "-fms-extensions");
            //     c_compile_cmd_add_flag(node, "-Wno-deprecated-declarations");
            //     c_compile_cmd_add_flag(node, "-Wno-microsoft-anon-tag");
            // }
        }
        if (node->type == link_cmd_type)
        {
            // LinkCmd* cmd = (LinkCmd*)node;
            // link_cmd_add_flag(node, "any string append to link cmd");
        }
    }
}

int main(int argc, char** argv)
{
    // Set to `true` to generate VSCode launch.json and tasks.json.
    // Even if it is not set to `true`, if the target `vsc_launch`(alias of `.vscode/launch.json`) is specified
    // on the command line, `.vscode/launch.json` will still be generated.
    set_generate_vscode_files_enabled(false);

    if (get_default_optimization() != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    set_after_prepare_callback(setup_default_flags);
    return execute();
}