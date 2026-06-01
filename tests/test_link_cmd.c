#include "core/string.h"
#include "cup/c_toolchain/link_cmd.h"
#include "cup/cup.h"
#include "cup/test.h"

void link_cmd_make_cmdline(Node* cmd);

static Node* add_fake_obj(char const* path)
{
    Node* src = SRC(path);
    Node* obj = OBJ(src);
    CC(src, obj);
    return obj;
}

TEST(test_link_cmd_llvm_link, link_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
    set_llvm_linker_type(LINKER_LLVM_LINK);

    Node* link = LINK(EXE("build/tests/link_cmd/llvm_lld"));
    link_cmd_add_input(link, add_fake_obj("tests/fake_link_cmd_lld.c"));

    node_ensure_prepared(link);
    link_cmd_make_cmdline(link);

    ASSERT(string_starts_with(link->cmdline, "clang "));
    ASSERT(string_contains(link->cmdline, "-fuse-ld=link"));
}

TEST(test_link_cmd_self_build_follows_llvm_linker, link_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_LLVM);
    set_llvm_linker_type(LINKER_LLVM_LINK);
    set_self_build_toolchain(TOOLCHAIN_TYPE_LLVM);

    Node* link = LINK(EXE("build/tests/link_cmd/self_build_lld"));
    link_cmd_set_linker_type(link, LINKER_LLVM_LD);
    link_cmd_setup_self_build(link);
    link_cmd_add_input(link, add_fake_obj("tests/fake_self_build_link_cmd_lld.c"));

    node_ensure_prepared(link);
    link_cmd_make_cmdline(link);

    ASSERT(string_starts_with(link->cmdline, "clang "));
    ASSERT(string_contains(link->cmdline, "-fuse-ld=link"));
}

TEST(test_link_cmd_llvm_linker_setting_skips_other_toolchains, link_cmd)
{
    set_default_toolchain(TOOLCHAIN_TYPE_GCC);
    set_llvm_linker_type(LINKER_LLVM_LLD);

    Node* link = LINK(EXE("build/tests/link_cmd/gcc_ignores_llvm_lld"));
    link_cmd_add_input(link, add_fake_obj("tests/fake_gcc_ignores_link_cmd_lld.c"));

    node_ensure_prepared(link);
    link_cmd_make_cmdline(link);

    ASSERT(string_starts_with(link->cmdline, "gcc "));
    ASSERT(!string_contains(link->cmdline, "-fuse-ld=lld"));
}
