#include "cup/cup.h"
#include "cup/cup.private.h"

char const* flag_sanitizer = NULL;
// char const* flag_sanitizer = "-fsanitize=address,undefined";

ENTRY(add_default_compile_flags, PRIORITY_AFTER_DEFAULT)
{
    size_t i = 0;
    for (Node* node; (node = enumerate_compile_cmd(&i));)
    {
        CCompileCmd* cmd = (CCompileCmd*)node;
        c_compile_cmd_add_include_directory(node, "src");
        c_compile_cmd_set_cpp_std(node, CPP_LANGUAGE_STANDARD_20);
        if (default_optimization_type == OPTIMIZATION_TYPE_DEBUG && cmd->toolchain == TOOLCHAIN_TYPE_LLVM)
        {
            if (flag_sanitizer)
            {
                c_compile_cmd_add_flag(node, flag_sanitizer);
                c_compile_cmd_add_flag(node, "-fno-omit-frame-pointer");
            }
            c_compile_cmd_add_flag(node, "-Wall");
            c_compile_cmd_add_flag(node, "-Wextra");
            c_compile_cmd_add_flag(node, "-Wno-unused-function");
            c_compile_cmd_add_flag(node, "-Wno-unused-parameter");
        }
        if (cmd->toolchain != TOOLCHAIN_TYPE_MSVC)
        {
            c_compile_cmd_add_flag(node, "-fms-extensions");
            c_compile_cmd_add_flag(node, "-Wno-deprecated-declarations");
            c_compile_cmd_add_flag(node, "-Wno-microsoft-anon-tag");
        }
        if (cmd->toolchain == TOOLCHAIN_TYPE_MSVC)
        {
            c_compile_cmd_set_c_std(node, C_LANGUAGE_STANDARD_23);
        }
    }
}

ENTRY(add_default_link_flags, PRIORITY_AFTER_PREPARE)
{
    size_t i = 0;
    for (Node* node; (node = enumerate_link_cmd(&i));)
    {
        LinkCmd* cmd = (LinkCmd*)node;
        if (cmd->toolchain == TOOLCHAIN_TYPE_LLVM && cmd->optimization == OPTIMIZATION_TYPE_DEBUG)
        {
            if (flag_sanitizer)
            {
                link_cmd_add_flag(node, flag_sanitizer);
            }
        }
    }
}

int main(int argc, char** argv)
{
    set_test_enabled(false);
    set_generate_vscode_files_enabled(false);
    if (get_default_optimization() != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    add_build_script_search_directory("tests");
    add_build_script_search_directory("src");
    return execute();
}
