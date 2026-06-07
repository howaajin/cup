---
name: opencode-cup-release
description: Use when OpenCode needs to build, test, or modify an external C/C++ project that uses only a Cup release artifact: build/embedded/cup.exe or build/header_only/cup.h. Covers first-time setup, embedded vs header-only detection, cup -hh, cached options, target paths, tests, ENTRY DAG editing, and correct macro recipes including LIB+AR for static libraries and EXE/DLL+LINK for linked binaries.
---

# Cup Release Usage

Use this skill in a consumer C/C++ project that uses Cup as its build system. Do not assume Cup source files exist. The user may only have one downloaded release artifact:

- `build/embedded/cup.exe` on Windows, or an equivalent executable `cup` on Linux/macOS.
- `build/header_only/cup.h`.

## OpenCode Location

Install this folder as `.opencode/skills/opencode-cup-release` for a project-local OpenCode skill, or as `~/.config/opencode/skills/opencode-cup-release` for a global OpenCode skill.

## First Steps

From the consumer project's root directory, detect how Cup is available:

1. If `cup.h` exists in the current directory, use header-only mode.
2. Otherwise, if `cup.exe` exists on Windows, use embedded mode.
3. Otherwise, on Linux/macOS, if an executable named `cup` or starting with `cup` can be run as `./cup...`, use embedded mode.
4. If neither is present, ask for the release artifact. If the user has `build/embedded/cup.exe`, run it from the project root or place it in the project root as `cup.exe`. If the user has `build/header_only/cup.h`, place it in the project root as `cup.h` and compile it.

If both `cup.h` and a Cup executable are present, treat the project as header-only mode.

Cup build scripts are C files named `build.c`. In both embedded and header-only release usage, build scripts include:

```c
#include "cup.h"
```

Do not include Cup source-tree paths such as `cup/cup.h` unless the user's project explicitly provides that layout.

## Run Cup

Embedded mode on Windows:

```pwsh
.\cup.exe -hh
.\cup.exe
```

Embedded mode on Linux/macOS:

```sh
./cup -hh
./cup
```

Header-only mode needs a one-time compile of `cup.h` with `MAIN_ENTRY`.

Windows with MSVC:

```pwsh
cl /Fe:cup.exe /Tc cup.h /std:clatest /DMAIN_ENTRY /nologo
.\cup.exe -hh
```

Linux:

```sh
clang -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions
./cup -hh
```

macOS:

```sh
clang -x c cup.h -DMAIN_ENTRY -o cup -fms-extensions
./cup -hh
```

Always run `cup -hh` for the actual artifact in the current project before relying on options. Key options:

- `-out_dir <dir>`: output directory, default `build`.
- `-t <toolchain>`: `llvm`, `msvc`, `gcc`, or `zig`.
- `-linker <linker>`: LLVM linker, usually `default` or `lld`.
- `-O0`, `-O3`, `-Os`: debug, fast release, size release.
- `-clean`: remove generated artifacts known to Cup.
- `-dry`: print/prepare work without executing commands.
- `-test [targets...]`: build and run test executables.
- `-root <dir>`: resolve relative paths from another root.

Cup records the last selected toolchain, optimization level, and LLVM linker in `{out_dir}/.last_status`. If later commands omit `-t`, `-O...`, or `-linker`, Cup reuses those saved values. Pass these flags explicitly when changing configuration or when reproducibility matters. Changing `-out_dir` changes where this status is read and written.

The `CUP_CLANG` environment variable overrides the LLVM toolchain's C/C++ compiler path:

```sh
export CUP_CLANG=/usr/lib/llvm-21/bin/clang-21
./cup -t llvm           # uses clang-21 / clang++-21
```

## Minimal build.c

Use this shape when creating or repairing a simple project:

```c
#include "cup.h"

ENTRY(build_app)
{
    Node* src = SRC("src/main.c");
    Node* obj = OBJ(src);
    CC(src, obj);

    Node* exe = EXE("{out_dir}/app");
    Node* link = LINK(exe);
    link_cmd_add_input(link, obj);
}

int main(int argc, char** argv)
{
    set_test_enabled(true);
    return execute();
}
```

Run it:

```pwsh
.\cup.exe
.\cup.exe build/app.exe
```

On Linux/macOS the executable target is usually `build/app`, because `EXE(...)` appends an empty executable extension there.

## Macro Rules

These macros are the high-signal surface area for everyday Cup build scripts:

- `SRC(fmt, ...)`: declare a C/C++ source node.
- `OBJ(src)`: get the default object node for a source.
- `CC(input, output)`: create a compile command, usually `CC(src, OBJ(src))`.
- `EXE(fmt, ...)`: declare an executable output and append `{exe_ext}`.
- `DLL(fmt, ...)`: declare a shared library output and append `{dll_ext}`.
- `LIB(fmt, ...)`: declare a static library output and append `{lib_ext}`.
- `FILE(fmt, ...)`: declare a normal file node, useful for headers, generated files, assets, stamps, and copy inputs/outputs.
- `LINK(output)`: create a linker command for an `EXE(...)` or `DLL(...)` output.
- `AR(output)`: create an archive/static-library command for a `LIB(...)` output.
- `CMD(command_line)`: create a custom external process command.
- `CMD_FROM_EXE(exe, name)`: create a command that runs an executable node built by Cup.
- `CALLBACK_CMD(fn, ctx)`: create an in-process C callback command.
- `COPY(src, dst)`: create a platform-specific copy command.

Mental model:

- `SRC`, `OBJ`, `EXE`, `DLL`, `LIB`, and `FILE` create or fetch nodes. They do not build anything by themselves.
- `CC`, `LINK`, `AR`, `CMD`, `CMD_FROM_EXE`, `CALLBACK_CMD`, and `COPY` create commands that build or update nodes.
- `EXE("{out_dir}/app")` only names the executable node; `LINK(EXE("{out_dir}/app"))` creates the link command.
- `LIB("{out_dir}/core")` only names the static library node; `AR(LIB("{out_dir}/core"))` creates the archive command.

Important rule: do not use `LINK()` to generate a static library. Static libraries are `LIB(...)` plus `AR(...)`. `LINK()` is for linked binaries: executables and DLL/shared libraries.

Correct:

```c
Node* lib = LIB("{out_dir}/core");
Node* ar = AR(lib);
ar_cmd_add_input(ar, obj);
```

Never write `LINK(lib)` or `LINK(LIB(...))` for a static library target.

## Common Recipes

Compile one source to one object:

```c
Node* src = SRC("src/foo.c");
Node* obj = OBJ(src);
CC(src, obj);
```

Build an executable:

```c
Node* src = SRC("src/main.c");
Node* obj = OBJ(src);
CC(src, obj);

Node* link = LINK(EXE("{out_dir}/app"));
link_cmd_add_input(link, obj);
```

Build a static library:

```c
Node* lib = LIB("{out_dir}/core");
Node* ar = AR(lib);

Node* src_a = SRC("src/a.c");
Node* obj_a = OBJ(src_a);
CC(src_a, obj_a);
ar_cmd_add_input(ar, obj_a);

Node* src_b = SRC("src/b.c");
Node* obj_b = OBJ(src_b);
CC(src_b, obj_b);
ar_cmd_add_input(ar, obj_b);
```

Build an executable that links objects and a static library:

```c
Node* lib = LIB("{out_dir}/core");
Node* ar = AR(lib);

Node* core_src = SRC("src/core.c");
Node* core_obj = OBJ(core_src);
CC(core_src, core_obj);
ar_cmd_add_input(ar, core_obj);

Node* app_src = SRC("src/main.c");
Node* app_obj = OBJ(app_src);
CC(app_src, app_obj);

Node* link = LINK(EXE("{out_dir}/app"));
link_cmd_add_input(link, app_obj);
link_cmd_add_input(link, lib);
```

Build a DLL/shared library:

```c
Node* src = SRC("src/plugin.c");
Node* obj = OBJ(src);
CC(src, obj);

Node* link = LINK(DLL("{out_dir}/plugin"));
link_cmd_add_input(link, obj);
```

Copy a file:

```c
Node* input = FILE("assets/config.json");
Node* output = FILE("{out_dir}/config.json");
COPY(input, output);
```

Run a generated tool:

```c
Node* gen = EXE("{out_dir}/gen_assets");
Node* run = CMD_FROM_EXE(gen, "generate assets");
cmd_add_output(run, FILE("{out_dir}/assets.c"));
```

## Build Targets

Cup accepts zero, one, or many target paths after options:

```pwsh
.\cup.exe
.\cup.exe build/core.lib
.\cup.exe build/core.lib build/app.exe
.\cup.exe -t llvm -O3 build/core.lib
```

Targets are node paths relative to the current root directory. Use Linux-style `/` separators even on Windows: `build/core.lib`, not `build\core.lib`.

The target path must match a node created by `build.c`. Examples:

- `EXE("{out_dir}/app")` becomes `build/app.exe` on Windows and `build/app` on Linux/macOS.
- `DLL("{out_dir}/plugin")` becomes `build/plugin.dll` on Windows and `build/plugin.so` on Linux.
- `LIB("{out_dir}/core")` becomes `build/core.lib` on Windows and `build/core.a` on Linux/macOS.
- `FILE("{out_dir}/generated/config.h")` becomes `build/generated/config.h`.

Use `-dry` when unsure:

```pwsh
.\cup.exe -dry build/core.lib
```

## Tests

Test sources include `cup/test.h` and declare test cases with `TEST(fn_name, group_name)` or `TEST(fn_name)`.

```c
#include "cup/test.h"

TEST(test_parser_accepts_empty_file, parser)
{
    ASSERT(true);
}
```

Compile test sources in `ENTRY` blocks:

```c
#include "cup.h"

ENTRY(build_test_parser)
{
    Node* src = SRC("{dir}/test_parser.c");
    Node* obj = OBJ(src);
    CC(src, obj);
    obj_add_link_obj_from_src(obj, SRC("src/parser.c"));
}

int main(int argc, char** argv)
{
    set_test_enabled(true);
    add_build_script("tests/build.c");
    return execute();
}
```

Run all tests:

```pwsh
.\cup.exe -test
```

Run one or more test executables by target path:

```pwsh
.\cup.exe -test build/tests/tests/xxx.c.exe
.\cup.exe -test build/tests/tests/aaa.c.exe build/tests/tests/bbb.c.exe
```

On Linux/macOS, test executable targets use `.test`:

```sh
./cup -test build/tests/tests/xxx.c.test
```

How Cup derives the test target path:

1. `-test` enables test scanning.
2. Cup scans compiled sources for `TEST(...)`.
3. For each compiled source with tests, Cup calls `add_test_exe_for_obj(obj, entries)`.
4. The source path comes from the object's compile command.
5. Windows test exe path: `{out_dir}/tests/<source-path>.exe`.
6. Linux/macOS test exe path: `{out_dir}/tests/<source-path>.test`.

Example: if `tests/build.c` compiles `SRC("{dir}/xxx.c")`, `{dir}` resolves to `tests`, so the source path is `tests/xxx.c`. With default `{out_dir}` of `build`, the Windows test target is `build/tests/tests/xxx.c.exe`. The repeated `tests/tests` is intentional: the first `tests` is Cup's test-output directory, and the second is the source path's `tests/` directory.

## ENTRY And DAG Edits

All build graph changes happen inside `ENTRY` functions. Add source, object, command, output, and dependency nodes there.

Use `{dir}` for paths relative to the `build.c` file that contains the current `ENTRY`. Use `{out_dir}` for generated outputs.

Register extra build scripts in `main`:

```c
int main(int argc, char** argv)
{
    set_test_enabled(true);
    add_build_script("src/build.c");
    add_build_script("tests/build.c");
    return execute();
}
```

Use priorities when an entry must run before or after normal node creation:

```c
ENTRY(set_project_vars, PRIORITY_BEFORE_DEFAULT)
{
    set_var("generated_dir", "{out_dir}/generated");
}

ENTRY(adjust_nodes, PRIORITY_AFTER_DEFAULT)
{
    /* Inspect or adjust nodes after normal entries created them. */
}
```

If all nodes need compile/link flag adjustments, prefer an `after_prepare` callback. Do not create new targets there; dependencies have already been determined.

```c
static void setup_flags(void)
{
    Node** nodes = get_all_nodes();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        if (nodes[i]->type == node_make_cmd_type(CMD_TYPE_EXECUTABLE, C_CMD_COMPILE))
        {
            c_compile_cmd_add_include_directory(nodes[i], "src");
        }
    }
}

int main(int argc, char** argv)
{
    set_after_prepare_callback(setup_flags);
    return execute();
}
```
