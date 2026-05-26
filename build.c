#include "cup/c_toolchain/ext_node_type.h"
#include "cup/cup.h"
#include "cup/cup.private.h"

bool b_enable_sanitizer = false;

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
            c_compile_cmd_add_include_directory(node, "src");
            c_compile_cmd_set_cpp_std(node, CPP_LANGUAGE_STANDARD_20);
            c_compile_cmd_set_c_std(node, C_LANGUAGE_STANDARD_23);
            CCompileCmd* cmd = (CCompileCmd*)node;
            if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
            {
                c_compile_cmd_add_flag(node, "-Wall");
                c_compile_cmd_add_flag(node, "-Wextra");
                c_compile_cmd_add_flag(node, "-Wno-unused-function");
                c_compile_cmd_add_flag(node, "-Wno-unused-parameter");
                if (cmd->optimization_type == OPTIMIZATION_TYPE_DEBUG && b_enable_sanitizer)
                {
                    c_compile_cmd_add_flag(node, "-fsanitize=address,undefined");
                    c_compile_cmd_add_flag(node, "-fno-omit-frame-pointer");
                }
            }
            if (cmd->toolchain != TOOLCHAIN_TYPE_MSVC)
            {
                c_compile_cmd_add_flag(node, "-fms-extensions");
                c_compile_cmd_add_flag(node, "-Wno-deprecated-declarations");
                c_compile_cmd_add_flag(node, "-Wno-microsoft-anon-tag");
            }
        }
        if (node->type == link_cmd_type)
        {
            LinkCmd* cmd = (LinkCmd*)node;
            if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM && cmd->optimization == OPTIMIZATION_TYPE_DEBUG && b_enable_sanitizer)
            {
                link_cmd_add_flag(node, "-fsanitize=address,undefined");
            }
        }
    }
}

int main(int argc, char** argv)
{
    set_test_enabled(true);
    set_generate_vscode_files_enabled(false);
    if (get_default_optimization() != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    add_build_script_search_directory("tests");
    add_build_script_search_directory("src");
    set_after_prepare_callback(setup_default_flags);
    return execute();
}
