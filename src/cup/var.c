#include "cup/var.h"
#include "core/ansi_color.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/platform.h"
#include "core/string.h"

char const* desc_color_exe = ANSI_COLOR_GREEN;
char const* desc_color_input = ANSI_COLOR_BLUE;
char const* desc_color_output = ANSI_COLOR_MAGENTA;
char const* desc_color_reset = ANSI_COLOR_RESET;
char const* desc_color_error = ANSI_COLOR_RED;
char const* desc_color_flag = ANSI_COLOR_GRAY;
char const* desc_color_bright_flag = ANSI_BRIGHT_YELLOW;

StringPtrHash* variables = NULL;

void var_on_cwd_changed(void)
{
    if (variables == NULL)
    {
        init_var();
    }
    else
    {
        Allocator* temp_allocator = allocator_temp();
        char const* cwd = os_get_cwd(temp_allocator);
        char const* self_exe_fullpath = os_get_current_exe_path(temp_allocator);
        if (path_is_under_directory(self_exe_fullpath, cwd))
        {
            self_exe_fullpath = path_lexically_relative(self_exe_fullpath, cwd, temp_allocator);
        }
        char const* self_name = path_stem(self_exe_fullpath, temp_allocator);
        set_var("self", self_exe_fullpath);
        set_var("self_name", self_name);
        set_var("workspace", cwd);
    }
}

void init_var(void)
{
    if (!os_is_terminal_supports_color())
    {
        desc_color_exe = "";
        desc_color_input = "";
        desc_color_output = "";
        desc_color_reset = "";
        desc_color_error = "";
        desc_color_flag = "";
        desc_color_bright_flag = "";
    }

    variables = allocator_calloc(allocator_c(), 1, sizeof(StringPtrHash));
    variables->allocator = allocator_c();
    struct
    {
        char const* key;
        char const* value;
    } map[] = {
        {"mode", "header_only"},
        {"out_dir", "build"},
        {"obj_dir", "obj"},
        {"exe_ext", EXE_EXT},
        {"dll_ext", DLL_EXT},
        {"lib_ext", LIB_EXT},
        {"obj_ext", OBJ_EXT},
        {"implib_ext", ".lib"},
        {"platform", get_platform_string(CURRENT_PLATFORM)},
        {"color_exe", desc_color_exe},
        {"color_in", desc_color_input},
        {"color_out", desc_color_output},
        {"color_flag", desc_color_flag},
        {"color_bright_flag", desc_color_bright_flag},
        {"color_reset", desc_color_reset},
        {"#", desc_color_reset},
    };
    for (size_t i = 0; i != static_array_size(map); i++)
    {
        set_var(map[i].key, map[i].value);
    }
    var_on_cwd_changed();
}

void set_var(char const* name, char const* value)
{
    uint32_t i = hash_index(variables, name);
    if (i != HASH_INVALID_INDEX)
    {
        if (value == NULL)
        {
            char const* key = hash_value(variables, i);
            char const* value = hash_key(variables, i);
            array_free(allocator_c(), key);
            array_free(allocator_c(), value);
            hash_remove(variables, i);
            return;
        }
        char* v = hash_value(variables, i);
        array_resize(allocator_c(), v, 0);
        string_concat_c_str(allocator_c(), v, value);
        hash_value(variables, i) = v;
        return;
    }
    char* k = string_from_c_str(allocator_c(), name);
    char* v = string_from_c_str(allocator_c(), value);
    hash_put(variables, k, v);
}

char const* get_var(char const* name)
{
    uint32_t i = hash_index(variables, name);
    if (i != HASH_INVALID_INDEX)
    {
        return hash_value(variables, i);
    }
    return NULL;
}

void destroy_var(void)
{
    if (!variables)
    {
        return;
    }
    StringPtrHash* h = variables;
    for (size_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* key = hash_key(h, i);
        array_free(allocator_c(), key);
        char const* value = hash_value(h, i);
        array_free(allocator_c(), value);
    }
    hash_free(h);
}
