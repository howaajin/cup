# Cup - A Build System in C

Cup is a build system implemented in C that uses C as its scripting language. Write your build logic in **C** — loops, functions, complex logic — with full support from your favorite tools (LSP, debugger).

Cup was originally inspired by the build system of Unreal Engine.

[Example build script](#example-buildc). For more real-world examples, search for the self-built entry function in this project: `ENTRY(`

## Features

* **Native C Build Scripts**: `build.c` is the build system. No DSL.
* **Self-Hosting**: Only depends on a C compiler. The initial build directly invokes the compiler, after which Cup supports automatic self-updating, even in debug mode.
* **Clear command-line output**: No build commands are hidden, input and output are highlighted for better readability, without losing the compiler’s colored output.
* **Platform Support**: **Windows**, **Linux**, and **macOS**.
* **IDE Support**:
    * Generates `compile_commands.json` for Clangd.
    * Generates **VS Code** `launch.json` and `tasks.json`.
    * Generates **Visual Studio** `.sln` / `.slnx` projects.
* **Caching**: Only rebuilds what changed.
* **Parallel Builds**: Executes independent commands simultaneously.
* **Callback-Based Build Commands**: Supports custom callback functions integrated into the build graph.
* **Header Dependencies**: Parses MSVC `/showIncludes` and GCC `-M` output.
* **C++20 Modules**: Supports automatic scanning and dependency resolution for named modules. Automatically builds `std` and `std.compat`. Modules can also be declared manually via `c_compile_cmd_set_export_name(name)` and imported with `c_compile_cmd_add_import(cc, name)`.
* **Lightweight**: Optimized `cup` / `cup.exe` builds are ~780 KB, while the standalone `cup.h` is ~540 KB.

## Non-goals

* **A package manager.**
* **Support for non-C/C++ languages.** To keep the project small and focused, native support for other languages is not considered. Although it can invoke external commands, its abstractions are intended only to simplify C or C++ projects.

## Modes

Cup can be used in two ways:

1. **Single Header Mode (`cup.h`)**: Amalgamated single header. Include it in your build script. Zero dependencies besides a C compiler.  Recompiles the build logic every time.

2. **Binary Mode (`cup`/`cup.exe`)**: Compile Cup once into a standalone executable. The executable loads your `build.c` as a dynamic library. Faster than `Single Header Mode`.

## Quick Start

1. Download the latest release for your platform (`cup`, `cup.exe`, or `cup.h`) from the Releases page.
2. Run the executable or compile `cup.h` with `-DMAIN_ENTRY` (see compile commands at the end of `cup.h`).
3. If no `build.c` exists, Cup generates a default one. Edit it as needed.

## Build from Source

Run `bootstrap.bat` (Windows) or `bootstrap.sh` (Linux/macOS). Generates the embedded binary in `build/embedded/`, the single-header distribution in `build/header_only/`, and a local `cup` / `cup.exe` executable in the project root for building this repository itself.

## Built-in Testing Framework

```c
#include "cup/test.h"

TEST(my_test_case) {
    ASSERT(1 + 1 == 2);
}
```

Use `cup -test` to run all registered tests.

You may also be interested in **[Cup for VS Code](https://github.com/howaajin/cup_vscode)**  a VS Code extension supports `Cup`. provides enhanced Test Explorer UI for discovery, execution, and debugging.

## Example `build.c`

```c
#include "cup.h"

// The `ENTRY` macro declares a build entry point. All entry points are executed when `execute()` is called.
ENTRY(build_hello) {
    // Declare a source file participating in the build.
    Node *src = SRC("src/hello.c");
    // Declare an object file participating in the build.
    Node *obj = OBJ(src);
    // Declare a compile command that compiles `src` into `obj`.
    CC(src, obj);

    Node *dep = OBJ(SRC("src/core/allocator.c"));
    // Specify that all commands depending on `obj` must also depend on `dep`.
    obj_add_link_node(obj, dep); 

    // Declare an executable file to be generated. `build/hello.exe` on Windows
    Node *exe = EXE("{out_dir}/hello"); 
    // Declare a link command with `exe` as its primary output.
    Node *link = LINK(exe);
    // Add `obj` as an input to the link command.
    link_cmd_add_input(link, obj);
}

// Manual source list
ENTRY(iterate_by_array) {
    char const *sources[] = { "a.c", "b.c", "c.c" };
    Node *exe = EXE("e");
    Node *link = LINK(exe);
    for (size_t i = 0; i != static_array_size(sources); i++) {
        Node *src = SRC(sources[i]);
        Node *obj = OBJ(src);
        CC(src, obj);
        link_cmd_add_input(link, obj);
    }
}

int main(int argc, char **argv) {
    // Enable built-in testing framework
    set_test_enabled(true);
    // Generate VS Code launch.json and tasks.json.
    set_generate_vscode_files_enabled(true);
    // Disable debug information for non-debug builds.
    if (get_default_optimization() != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    // Search for build entry files under the src directory.
    add_build_script_search_directory("src");
    return execute();
}
```

## Contributing

Issues and Pull Requests are welcome to improve Cup!
