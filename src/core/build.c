#include "core/array.h"
#include "core/macros.h"
#include "cup/c_toolchain/c_toolchain.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/entry.h"
#include "cup/node.h"
#include "cup/var.h"

#include <assert.h>

extern ToolchainType default_toolchain;

static void gen_hash_h_prepare(Node* cmd_gen_hash)
{
    cmd_prepare(cmd_gen_hash);
    Node** nodes = get_all_nodes();
    Node* hash_h = cmd_gen_hash->outputs[0];
    Node* cc = cmd_gen_hash->extra_data;
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_CMD)
        {
            continue;
        }
        if (node->cmd_type != CMD_TYPE_EXECUTABLE || node->cmd_ext_type != C_CMD_COMPILE)
        {
            continue;
        }
        if (cc == node)
        {
            continue;
        }
        node_add_dependency(node, hash_h);
    }
}

ENTRY(build_hash_h)
{
    Node* exe = EXE("{out_dir}/gen_hash");
    Node* link = LINK(exe);
    link_cmd_setup_self_build(link);

    Node* src = get_or_add_src("src/core/gen_hash.c");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
    }
    link_cmd_add_input(link, obj);

    Node* output = get_or_add_file("src/core/hash.h");
    Node* input = get_or_add_src("src/core/gen_hash.c");

    Node* cmd_gen_hash = CMD_FROM_EXE(exe, fmt("gen: {:n}", output));
    cmd_gen_hash->prepare = gen_hash_h_prepare;
    cmd_gen_hash->extra_data = cc;
    cmd_add_option(cmd_gen_hash, OPTION_FLAG, "-o");
    cmd_add_output_file_option(cmd_gen_hash, output);
    cmd_add_input_file_option(cmd_gen_hash, input);
}
ENTRY(build_path)
{
    Node* src = SRC("src/core/path.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/allocator"));
}

ENTRY(build_utilities)
{
    Node* src = SRC("src/core/utilities.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(set_var_core, PRIORITY_BEFORE_DEFAULT)
{
#if CURRENT_PLATFORM == PLATFORM_WINDOWS
    set_var("platform_directory_c", fmt("{dir}/windows/directory.c"));
#elif CURRENT_PLATFORM == PLATFORM_LINUX || CURRENT_PLATFORM == PLATFORM_MACOS
    set_var("platform_directory_c", fmt("{dir}/common/directory.c"));
#endif
}

ENTRY(build_directory)
{
    Node* src = SRC("{platform_directory_c}");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
    }
}

ENTRY(build_dylib)
{
    Node* src = SRC("{dir}/{platform}/dylib.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_platform_os)
{
    Node* src = SRC("{dir}/{platform}/os.c");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        c_compile_cmd_add_define(cc, "_CRT_SECURE_NO_WARNINGS");
    }
    ToolchainType toolchain = default_toolchain;
    if (toolchain == TOOLCHAIN_TYPE_GCC &&
        CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        c_compile_cmd_add_define(cc, "_WIN32_WINNT=0x0A00");
        obj_add_link_lib(obj, "userenv");
    }
    if (toolchain == TOOLCHAIN_TYPE_ZIG)
    {
        obj_add_link_lib(obj, "userenv");
    }
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        c_compile_cmd_add_define(cc, "_GNU_SOURCE");
    }
    if (CURRENT_PLATFORM == PLATFORM_LINUX || CURRENT_PLATFORM == PLATFORM_MACOS)
    {
        Node* src_common = SRC("{dir}/common/os.c");
        Node* obj_common = OBJ(src_common);
        CC(src_common, obj_common);
        obj_add_link_node(obj, obj_common);
    }
}

ENTRY(build_json)
{
    Node* src = SRC("{dir}/json.c");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_os)
{
    Node* src = SRC("{dir}/os.c");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("{dir}/{platform}/os.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/path.c"));
    obj_add_link_obj_from_src(obj, SRC("{dir}/utilities.c"));
    if (CURRENT_PLATFORM == PLATFORM_LINUX)
    {
        c_compile_cmd_add_define(cc, "_GNU_SOURCE");
    }
    if (CURRENT_PLATFORM == PLATFORM_WINDOWS)
    {
        if (default_toolchain == TOOLCHAIN_TYPE_GCC || default_toolchain == TOOLCHAIN_TYPE_ZIG)
        {
            obj_add_link_lib(obj, "bcrypt");
        }
    }
}

ENTRY(build_core_lib)
{
    extern ToolchainType self_build_toolchain;

    Node* inputs[] = {
        OBJ(SRC("{dir}/os.c")),
        OBJ(SRC("{dir}/json.c")),
        OBJ(SRC("{dir}/path.c")),
        OBJ(SRC("{dir}/utilities.c")),
        OBJ(SRC("{dir}/{platform}/dylib.c")),
        OBJ(SRC("{dir}/{platform}/os.c")),
        OBJ(SRC("{platform_directory_c}")),
        LIB("{out_dir}/allocator.lib"),
    };
    Node* core_lib = LIB("{out_dir}/core");
    Node* cmd = AR(core_lib);
    ar_cmd_set_toolchain_type(cmd, self_build_toolchain);
    for (size_t i = 0; i != static_array_size(inputs); i++)
    {
        Node* input = inputs[i];
        ar_cmd_add_input(cmd, input);
    }
}

#include "allocators/build.c"