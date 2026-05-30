#include "cup/bootstrap.c"

#include "cup/bin2c/build_script_tpl.c.c"
#include "cup/bin2c/test.c.c"
#include "cup/bin2c/test.h.c"
#include "cup/bin2c/test_main.c.c"
#include "cup/embedded_file.c"
#include "cup/gen_build_c.c"
#include "cup/gen_test_src.c"
#include "cup/header_only.c"

// Compile commands:
// Windows:
// clang -x c cup.h -DMAIN_ENTRY -o cup.exe
// gcc -x c cup.h -DMAIN_ENTRY -luserenv -lbcrypt -o cup.exe
// cl /Fe:cup.exe /Tc cup.h /std:clatest /experimental:c11atomics /DMAIN_ENTRY
// cl /Fe:cup.exe /Tc cup.h /std:clatest /experimental:c11atomics /DMAIN_ENTRY /Od /nologo /Z7 /link /debug /incremental:no /noexp /noimplib /pdb:build/cup.exe.pdb
// Linux/macOS(-D_GNU_SOURCE can be omitted on macOS):
// gcc -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions
// clang -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions

#ifdef MAIN_ENTRY
int main(int argc, char** argv)
{
    set_default_toolchain(get_toolchain_by_current_compiler());
    OptimizationType optimization = OPTIMIZATION_TYPE_DEBUG;
    set_default_optimization(optimization);
    if (optimization != OPTIMIZATION_TYPE_DEBUG)
    {
        set_debug_info_enabled(false);
    }
    return execute();
}
#endif
