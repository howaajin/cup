#include "cup/cup.h"
#include "cup/cup.private.h"

ENTRY(build_test_graph)
{
    Node* src = SRC("{dir}/test_graph.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/core/allocator.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/graph.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/node.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/in_repo.c"));
}

ENTRY(build_test_c_compile_cmd)
{
    Node* src = SRC("{dir}/test_c_compile_cmd.c");
    Node* obj = get_default_obj(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_compile_cmd.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/c_toolchain.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/fmt.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/in_repo.c"));
}

ENTRY(build_test_string)
{
    Node* src = SRC("{dir}/test_string.c");
    Node* obj = get_default_obj(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/core/allocator.c"));
}

ENTRY(build_test_json)
{
    Node* src = SRC("{dir}/test_json.c");
    Node* obj = get_default_obj(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/core/allocator.c"));
    obj_add_link_obj_from_src(obj, SRC("src/core/json.c"));
}

ENTRY(build_test_executor)
{
    Node* src = SRC("{dir}/test_executor.c");
    Node* obj = get_default_obj(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/cup/executor/executor_{platform}.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/fmt.c"));
}

ENTRY(build_test_header_only)
{
    Node* src = SRC("{dir}/test_header_only.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/cup"));
    obj_add_link_node(obj, LIB("{out_dir}/core"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/in_repo.c"));
}

ENTRY(build_test_binary_mode)
{
    Node* src = SRC("{dir}/test_binary_mode.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/cup"));
    obj_add_link_node(obj, LIB("{out_dir}/core"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/in_repo.c"));
}