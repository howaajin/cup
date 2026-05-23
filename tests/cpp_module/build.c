#include "cup/cup.h"
#include "cup/cup.private.h"

bool b_build_cpp_tests = false;

bool build_cpp_tests()
{
    if (default_toolchain == TOOLCHAIN_TYPE_ZIG || !b_build_cpp_tests)
    {
        return false;
    }
    return true;
}

ENTRY(build_module1)
{
    if (!build_cpp_tests())
    {
        return;
    }
    Node* src = SRC("{dir}/module1.cppm");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_module2)
{
    if (!build_cpp_tests())
    {
        return;
    }
    Node* src = SRC("{dir}/module2.cppm");
    Node* obj = OBJ(src);
    CC(src, obj);
}

ENTRY(build_test_cpp_module_main)
{
    if (!build_cpp_tests())
    {
        return;
    }
    Node* src = SRC("{dir}/main.cpp");
    Node* obj = OBJ(src);
    Node* cc = CC(src, obj);
    (void)cc;
    Node* link = LINK(EXE("{out_dir}/tests/cpp_module/main"));
    link_cmd_add_input(link, obj);
}

ENTRY(build_test_scan_deps_cmd)
{
    Node* src = SRC("{dir}/test_scan_deps_cmd.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_node(obj, LIB("{out_dir}/core"));
    obj_add_link_node(obj, LIB("{out_dir}/cup"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/in_repo.c"));
    obj_add_link_obj_from_src(obj, SRC("src/cup/c_toolchain/scan_deps_cmd.c"));
}