#include "cup.h"

int main(int argc, char** argv)
{
    // Set to `true` to generate VSCode launch.json and tasks.json.
    set_generate_vscode_files_enabled(false);

    OptimizationType optimization = OPTIMIZATION_TYPE_DEBUG;
    set_default_optimization(optimization);
    if (optimization != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    return execute();
}