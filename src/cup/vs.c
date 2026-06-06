#include "core/array.h"
#include "core/hash.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/c_toolchain/c_compile_cmd.h"
#include "cup/c_toolchain/ext_node_type.h"
#include "cup/entry.h"
#include "cup/graph.h"
#include "cup/var.h"

typedef struct Graph Graph;
typedef struct Vcxproj Vcxproj;
typedef struct VcxprojFilter VcxprojFilter;
typedef struct StringSet StringSet;
typedef struct StringPtrHash StringPtrHash;
typedef struct SlnFile SlnFile;

char const* vs_version = "vs2026";
char const* platform_toolset = "v145";

struct VcxprojFilter
{
    char const* path;
    char const* name;
    char const* guid;
};

struct SlnFile
{
    Allocator* allocator;
    char const* version;
    Vcxproj** projects;
    char const* configuration;
    char const* architecture;
    StringPtrHash* hash_folder_to_parent_guid;
    StringPtrHash* hash_folder_path_to_guid;
};

struct Vcxproj
{
    Allocator* allocator;
    char* name;
    char* path;
    char* guid;
    char const* config_name;
    char const* arch_name;
    char const* target_path;
    char* folder_path;
    CLanguageStandard c_std;
    CppLanguageStandard cxx_std;
    char const** sources;
    char const** headers;
    char const** defines;
    char const** includes;
    char const* debugger_working_dir;
    char const** debugger_args;
    Vcxproj** dependencies;
    StringSet* hash_set_files;
    StringSet* hash_set_defines;
    StringSet* hash_set_includes;
    StringPtrHash* hash_source_to_defines;
    StringPtrHash* hash_source_to_includes;
    bool b_exclude_in_build;
    bool b_executable;
};

static Vcxproj* vcxproj_create(
    Allocator* allocator,
    char* name,
    char* path,
    char* guid,
    char const* config_name,
    char const* arch_name,
    char* target_path,
    char* folder_path,
    char const* debugger_working_dir,
    char const** debugger_args)
{
    Vcxproj* p = allocator_calloc(allocator, 1, sizeof(Vcxproj));
    p->allocator = allocator;
    p->hash_set_files = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_files->allocator = allocator;
    p->hash_set_defines = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_defines->allocator = allocator;
    p->hash_set_includes = allocator_calloc(allocator, 1, sizeof(StringSet));
    p->hash_set_includes->allocator = allocator;
    p->hash_source_to_defines = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    p->hash_source_to_defines->allocator = allocator;
    p->hash_source_to_includes = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    p->hash_source_to_includes->allocator = allocator;
    p->name = name;
    p->path = path;
    p->guid = guid;
    p->config_name = config_name;
    p->arch_name = arch_name;
    p->target_path = target_path;
    p->folder_path = folder_path;
    p->debugger_working_dir = debugger_working_dir;
    p->debugger_args = debugger_args;
    if (string_equal(p->arch_name, "x86"))
    {
        p->arch_name = "Win32";
    }
    return p;
}

static void vcxproj_file_peek(FILE* file, char* buffer, uint64_t size)
{
    long o = ftell(file);
    size_t num_read = fread(buffer, 1, size, file);
    (void)num_read;
    if (fseek(file, o, SEEK_SET) != 0)
    {
        error("fseek error!");
    }
}
static void vcxproj_file_eat(FILE* file, uint64_t size)
{
    if (fseek(file, (long)size, SEEK_CUR) != 0)
    {
        error("fseek error!");
    }
}

static char* vcxproj_guid_from_existed_project(char const* path, Allocator* allocator)
{
    char* result = NULL;
    Allocator* arena_allocator = allocator_create_chained();
    FILE* cs = os_fopen(path, "rt");
    char tag[] = "<ProjectGuid>";
    uint64_t tag_len = sizeof(tag) - 1;
    bool found = false;
    for (;;)
    {
        char peek_buffer[sizeof(tag) - 1];
        vcxproj_file_peek(cs, peek_buffer, tag_len);
        if (peek_buffer[0] == EOF)
        {
            break;
        }
        found = true;
        for (uint64_t i = 0; i != tag_len; i++)
        {
            if (tag[i] != peek_buffer[i])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            vcxproj_file_eat(cs, tag_len + 1);
            break;
        }
        else
        {
            vcxproj_file_eat(cs, 1);
        }
    }

    if (!found)
    {
        goto Cleanup;
    }

    for (;;)
    {
        char ch;
        vcxproj_file_peek(cs, &ch, 1);
        if (ch != '}')
        {
            array_push(allocator, result, (char)ch);
            vcxproj_file_eat(cs, 1);
        }
        else
        {
            goto Success;
        }
    }
Cleanup:
    result = NULL;
Success:
    fclose(cs);
    allocator_destroy(arena_allocator);
    string_putc(allocator, result, '\0');
    return result;
}

static char* vcxproj_sln_guid_from_file(char const* path, Allocator* allocator)
{
    char* result = NULL;
    Allocator* arena_allocator = allocator_create_chained();
    FILE* fs = os_fopen(path, "rt");
    assert(fs);
    char tag[] = "SolutionGuid";
    uint64_t tag_len = sizeof(tag) - 1;
    bool found = false;
    for (;;)
    {
        char peek_buffer[sizeof(tag) - 1];
        vcxproj_file_peek(fs, peek_buffer, tag_len);
        if (peek_buffer[0] == EOF)
        {
            break;
        }
        found = true;
        for (uint64_t i = 0; i != tag_len; i++)
        {
            if (tag[i] != peek_buffer[i])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            vcxproj_file_eat(fs, tag_len + 1);
            break;
        }
        else
        {
            vcxproj_file_eat(fs, 1);
        }
    }
    if (!found)
    {
        goto Cleanup;
    }
    for (;;)
    {
        char ch;
        vcxproj_file_peek(fs, &ch, 1);
        vcxproj_file_eat(fs, 1);
        if (ch == '{')
        {
            break;
        }
    }
    for (;;)
    {
        char ch;
        vcxproj_file_peek(fs, &ch, 1);
        if (ch != '}')
        {
            array_push(allocator, result, (char)ch);
            vcxproj_file_eat(fs, 1);
        }
        else
        {
            goto Success;
        }
    }
Cleanup:
    result = NULL;
Success:
    fclose(fs);
    allocator_destroy(arena_allocator);
    string_putc(allocator, result, '\0');
    return result;
}

static char* vcxproj_find_or_create_guid(char const* path, Allocator* allocator)
{
    char* guid = NULL;
    if (os_file_exists(path))
    {
        guid = vcxproj_guid_from_existed_project(path, allocator);
    }
    if (!guid)
    {
        return os_create_guid(allocator, false);
    }
    return guid;
}

static char* sln_find_or_create_guid(char const* path, Allocator* allocator)
{
    char* guid = NULL;
    if (os_file_exists(path))
    {
        guid = vcxproj_sln_guid_from_file(path, allocator);
    }
    if (!guid)
    {
        return os_create_guid(allocator, false);
    }
    return guid;
}

static void vcxproj_add_include_dir(Vcxproj* project, char const* include_dir)
{
    if (hash_key_existed(project->hash_set_includes, include_dir))
    {
        return;
    }
    Allocator* temp_allocator = allocator_temp();
    char const* full_path = os_full_path(include_dir, temp_allocator);
    char* dir = string_from_c_str(project->allocator, full_path);
    path_slash_to_backslash(dir);
    array_push(project->allocator, project->includes, dir);
    hash_insert(project->hash_set_includes, (char const*)dir);
}
static char const* vcxproj_find_header_for_source(char const* source, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* workspace = get_var("workspace");
    char const* src_stem = path_stem(source, temp_allocator);
    char const* parent = path_parent_path(source, temp_allocator);
    char const* full_parent_parent = NULL;
    char const* parent_parent = NULL;
    char const* header = NULL;
    if (parent[0] == 0)
    {
        goto NOT_FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.h", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.inl", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.hpp", parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    parent_parent = path_parent_path(parent, temp_allocator);
    full_parent_parent = os_full_path(parent_parent, temp_allocator);
    if (!path_is_under_directory(full_parent_parent, workspace))
    {
        goto NOT_FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.h", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.inl", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
    header = string_from_print(temp_allocator, "%s/%s.hpp", parent_parent, src_stem);
    if (os_file_exists(header))
    {
        goto FOUND;
    }
NOT_FOUND:
    allocator_destroy(temp_allocator);
    return NULL;
FOUND:
    header = string_from_c_str(allocator, header);
    allocator_destroy(temp_allocator);
    return header;
}

static char const* path_to_vs_path(char const* path)
{
    if (!path_is_absolute(path))
    {
        char const* workspace = get_var("workspace");
        path = path_combine(allocator_temp(), workspace, path, NULL);
    }
    else
    {
        path = string_from_c_str(allocator_temp(), path);
    }
    path_slash_to_backslash((char*)path);
    return path;
}

void vcxproj_add_header(Vcxproj* project, char const* header)
{
    if (hash_key_existed(project->hash_set_files, header))
    {
        return;
    }
    hash_insert(project->hash_set_files, header);
    array_push(project->allocator, project->headers, header);
}

void vcxproj_add_source(Vcxproj* project, char const* source)
{
    if (hash_key_existed(project->hash_set_files, source))
    {
        return;
    }
    hash_insert(project->hash_set_files, source);

    array_push(project->allocator, project->sources, source);

    char const* header = vcxproj_find_header_for_source(source, project->allocator);
    if (header)
    {
        vcxproj_add_header(project, header);
    }
}

void vcxproj_add_define_for_source(Vcxproj* project, char const* source, char const* define)
{
    if (hash_key_existed(project->hash_set_defines, define))
    {
        return;
    }
    bool b_existed;
    uint32_t index = hash_insert_check(project->hash_source_to_defines, source, &b_existed);
    StringSet* defines;
    if (!b_existed)
    {
        defines = allocator_calloc(project->allocator, 1, sizeof(StringSet));
        defines->allocator = project->allocator;
        hash_value(project->hash_source_to_defines, index) = defines;
    }
    else
    {
        defines = (StringSet*)hash_value(project->hash_source_to_defines, index);
    }
    char const* key = string_from_c_str(project->allocator, define);
    hash_insert(defines, key);
}

void vcxproj_add_include_dir_for_source(Vcxproj* project, char const* source, char const* include_dir)
{
    if (hash_key_existed(project->hash_set_includes, include_dir))
    {
        return;
    }
    bool b_existed;
    uint32_t index = hash_insert_check(project->hash_source_to_includes, source, &b_existed);
    StringSet* includes;
    if (!b_existed)
    {
        includes = allocator_calloc(project->allocator, 1, sizeof(StringSet));
        includes->allocator = project->allocator;
        hash_value(project->hash_source_to_includes, index) = includes;
    }
    else
    {
        includes = (StringSet*)hash_value(project->hash_source_to_includes, index);
    }
    hash_insert(includes, include_dir);
}

extern OptimizationType default_optimization_type;
extern ArchitectureType default_architecture_type;
extern CLanguageStandard default_c_std;
extern CppLanguageStandard default_cpp_std;

static Vcxproj* target_vcxproj_from_node(Node* node, Allocator* allocator)
{
    if (node->node_type != NODE_TYPE_CMD)
    {
        return NULL;
    }
    if (node->cmd_type == CMD_TYPE_EXECUTABLE && node->cmd_ext_type == C_CMD_COMPILE)
    {
        return NULL;
    }
    if (array_size(node->outputs) == 0)
    {
        return NULL;
    }
    Allocator* temp_allocator = allocator_create_chained();
    Node* output = node->outputs[0];
    char* target_path = output->path;
    char const* dir = path_parent_path(target_path, temp_allocator);
    char* sln_folder = dir[0] ? string_from_print(allocator, "targets\\%s", dir) : "targets";
    char const* out_dir = get_var("out_dir");
    char* project_name = path_filename(target_path, allocator);
    char* project_path = string_from_print(allocator, "%s\\ProjectFiles\\%s\\%s.vcxproj", out_dir, sln_folder, project_name);
    char const* cfg_name = get_optimization_string(default_optimization_type);
    char const* cwd = get_var("workspace");
    path_slash_to_backslash(project_path);
    Vcxproj* p = vcxproj_create(
        allocator,
        project_name,
        project_path,
        vcxproj_find_or_create_guid(project_path, allocator),
        cfg_name,
        get_arch_string(default_architecture_type),
        target_path,
        sln_folder,
        cwd, output->debugger_run_arguments);
    allocator_destroy(temp_allocator);
    p->b_executable = output->file_type == FILE_TYPE_EXE;
    return p;
}

static void sln_add_project(SlnFile* sln, Vcxproj* p)
{
    if (!sln->architecture)
    {
        sln->architecture = string_from_c_str(sln->allocator, p->arch_name);
    }
    if (!sln->configuration)
    {
        sln->configuration = string_from_c_str(sln->allocator, p->config_name);
    }
    if (!string_equal(sln->architecture, p->arch_name))
    {
        warn("Only one platform is supported at a time!");
        warn("    Old: %s", sln->configuration);
        warn("    New: %s", p->arch_name);
    }
    if (!string_equal(sln->configuration, p->config_name))
    {
        warn("Only one configuration is supported at a time!");
        warn("    Old: %s", sln->configuration);
        warn("    New: %s", p->config_name);
    }
    array_push(sln->allocator, sln->projects, p);
}

static void vcxproj_add_dependency(Vcxproj* p, Vcxproj* dep)
{
    Vcxproj* f = array_find(p->dependencies, array_pointer_compare, &dep);
    if (!f)
    {
        array_push(p->allocator, p->dependencies, dep);
    }
}

static void vcxproj_set_deps(Vcxproj* p, Node* cmd, Hash* hash_node_to_vcxproj)
{
    Allocator* temp_allocator = allocator_create_chained();
    Node** stack = NULL;
    Set set = {.allocator = temp_allocator};
    array_push(temp_allocator, stack, cmd);
    while (array_size(stack) != 0)
    {
        Node* node = array_pop(stack);
        bool b_existed;
        hash_insert_check(&set, (uintptr_t)node, &b_existed);
        if (b_existed)
        {
            continue;
        }
        Vcxproj* dependency = (Vcxproj*)(uintptr_t)hash_get(hash_node_to_vcxproj, (uintptr_t)node);
        if (dependency && (dependency != p))
        {
            vcxproj_add_dependency(p, dependency);
        }
        for (size_t i = 0; i != array_size(node->dependencies); i++)
        {
            Node* prev = node->dependencies[i];
            array_push(temp_allocator, stack, prev);
        }
    }
    allocator_destroy(temp_allocator);
}

static char* vcxproj_sln_add_folder_path(StringPtrHash* hash, StringPtrHash* hash_folder_to_parent_guid, char const* path, Allocator* allocator)
{
    if (!path_has_relative_path(path))
    {
        return NULL;
    }
    char* parent = path_parent_path(path, allocator);
    char* parent_guid = vcxproj_sln_add_folder_path(hash, hash_folder_to_parent_guid, parent, allocator);
    bool b_existed;
    uint32_t index = hash_insert_check(hash, path, &b_existed);
    char* guid;
    if (!b_existed)
    {
        guid = os_create_guid(allocator, false);
        char const* key = string_from_c_str(allocator, path);
        hash_value(hash, index) = guid;
        hash_key(hash, index) = key;
    }
    else
    {
        guid = hash_value(hash, index);
    }
    if (parent_guid)
    {
        hash_put(hash_folder_to_parent_guid, guid, parent_guid);
    }
    return guid;
}

static void vcxproj_make_sln_folder_hash(Vcxproj** projects, StringPtrHash* hash_folder_to_parent_guid, StringPtrHash* hash_folder_path_to_guid, Allocator* allocator)
{
    for (uint64_t i = 0; i != array_size(projects); i++)
    {
        Vcxproj* p = projects[i];
        if (p->folder_path)
        {
            vcxproj_sln_add_folder_path(hash_folder_path_to_guid, hash_folder_to_parent_guid, p->folder_path, allocator);
        }
    }
}

static char* vcxproj_get_command(Vcxproj const* project, Allocator* allocator)
{
    return fmt_alloc(allocator, "cd $(SolutionDir) &amp;&amp; {self_name}");
}

static char* vcxproj_get_build_command(Vcxproj const* project, Allocator* allocator)
{
    char* cmd = vcxproj_get_command(project, allocator);
    if (project->target_path)
    {
        return string_from_print(allocator, "%s %s", cmd, project->target_path);
    }
    else
    {
        return string_from_print(allocator, "%s", cmd);
    }
}
static char const* vcxproj_get_clean_command(Vcxproj const* project, Allocator* allocator)
{
    char* clean_command = vcxproj_get_build_command(project, allocator);
    string_concat_c_str(allocator, clean_command, " -clean");
    return clean_command;
}
static char const* vcxproj_get_compile_command(Vcxproj const* project, Allocator* allocator)
{
    char* cmd = vcxproj_get_command(project, allocator);
    string_concat_c_str(allocator, cmd, " -compile \"$(SelectedFiles)\"");
    return cmd;
}

static char* vcxproj_concat_strings(char const** strings, Allocator* allocator)
{
    char* result = NULL;
    uint64_t num = array_size(strings);
    for (uint64_t i = 0; i != num; i++)
    {
        string_concat_c_str(allocator, result, strings[i]);
        if (i != num - 1)
        {
            array_push(allocator, result, ';');
        }
    }
    array_push(allocator, result, '\0');
    array_pop(result);
    return result;
}
static char* vcxproj_get_source_defines(Vcxproj const* project, char const* source, Allocator* allocator)
{
    StringSet* defines = (StringSet*)hash_get(project->hash_source_to_defines, source);
    char* result = NULL;
    if (defines)
    {
        for (uint64_t i = defines->begin; i != defines->end; i++)
        {
            if (hash_index_existed(defines, i))
            {
                char const* define = hash_key(defines, i);
                if (result)
                {
                    string_putc(allocator, result, ';');
                }
                string_concat_c_str(allocator, result, define);
            }
        }
    }
    return result;
}

static char* vcxproj_get_source_includes(Vcxproj const* project, char const* source, Allocator* allocator)
{
    StringSet* includes = (StringSet*)hash_get(project->hash_source_to_includes, source);
    if (!includes)
    {
        return NULL;
    }
    char* result = NULL;
    for (uint32_t i = includes->begin; i != includes->end; i = hash_next(includes, i))
    {
        char const* include = hash_key(includes, i);
        if (result)
        {
            string_putc(allocator, result, ';');
        }
        string_printf(allocator, result, "$(SolutionDir)%s", include);
    }
    return result;
}

char const* vcxproj_to_string(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    char const* out_dir = get_var("out_dir");

    char* xml = NULL;
    string_printf(allocator, xml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    string_printf(allocator, xml, "<Project DefaultTargets=\"Build\" ToolsVersion=\"16.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");
    // PropertyGroup
    string_printf(allocator, xml, "    <PropertyGroup>\n");
    string_printf(allocator, xml, "        <ProjectName>%s</ProjectName>\n", project->name);
    string_printf(allocator, xml, "        <ProjectGuid>{%s}</ProjectGuid>\n", project->guid);
    string_printf(allocator, xml, "        <PlatformToolset>%s</PlatformToolset>\n", platform_toolset);
    string_printf(allocator, xml, "        <ConfigurationType>Makefile</ConfigurationType>\n");
    string_printf(allocator, xml, "        <OutDir>$(SolutionDir)%s\\</OutDir>\n", out_dir);
    string_printf(allocator, xml, "        <NMakeBuildCommandLine>%s</NMakeBuildCommandLine>\n", vcxproj_get_build_command(project, arena_allocator));
    string_printf(allocator, xml, "        <NMakeCleanCommandLine>%s</NMakeCleanCommandLine>\n", vcxproj_get_clean_command(project, arena_allocator));
    if (project->includes)
    {
        char const* includes = vcxproj_concat_strings(project->includes, allocator);
        string_printf(allocator, xml, "        <NMakeIncludeSearchPath>%s</NMakeIncludeSearchPath>\n", includes);
    }
    string_printf(allocator, xml, "    </PropertyGroup>\n");
    if (project->sources && array_size(project->sources) > 0)
    {
        string_printf(allocator, xml, "    <ItemDefinitionGroup>\n");
        string_printf(allocator, xml, "        <NMakeCompile>\n");
        string_printf(allocator, xml, "            <NMakeCompileFileCommandLine>%s</NMakeCompileFileCommandLine>\n", vcxproj_get_compile_command(project, arena_allocator));
        string_printf(allocator, xml, "        </NMakeCompile>\n");
        string_printf(allocator, xml, "    </ItemDefinitionGroup>\n");
    }

    // Configuration Platform
    string_printf(allocator, xml, "    <ItemGroup>\n");
    string_printf(allocator, xml, "        <ProjectConfiguration Include=\"%s|%s\">\n", project->config_name, project->arch_name);
    string_printf(allocator, xml, "            <Configuration>%s</Configuration>\n", project->config_name);
    string_printf(allocator, xml, "            <Platform>%s</Platform>\n", project->arch_name);
    string_printf(allocator, xml, "        </ProjectConfiguration>\n");
    string_printf(allocator, xml, "    </ItemGroup>\n");
    // Import
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n");
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n");
    // Files
    string_printf(allocator, xml, "    <ItemGroup>\n");
    if (project->sources && array_size(project->sources) > 0)
    {
        for (uint64_t i = 0; i != array_size(project->sources); i++)
        {
            string_printf(allocator, xml, "        <ClCompile Include=\"%s\">\n", path_to_vs_path(project->sources[i]));
            if (string_ends_with(project->sources[i], ".cpp"))
            {
                string_printf(allocator, xml, "            <AdditionalOptions>/TP</AdditionalOptions>\n");
            }
            else
            {
                string_printf(allocator, xml, "            <AdditionalOptions>/TC</AdditionalOptions>\n");
            }
            // Add defines
            {
                char* defines = vcxproj_get_source_defines(project, project->sources[i], allocator);
                if (defines)
                {
                    string_printf(allocator, xml, "            <PreprocessorDefinitions>%s;%%(PreprocessorDefinitions)</PreprocessorDefinitions>\n", defines);
                    array_free(allocator, defines);
                }
            }
            // Add includes
            {
                char* includes = vcxproj_get_source_includes(project, project->sources[i], allocator);
                if (includes)
                {
                    string_printf(allocator, xml, "            <AdditionalIncludeDirectories>%s;%%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n", includes);
                    array_free(allocator, includes);
                }
            }
            string_printf(allocator, xml, "        </ClCompile>\n");
        }
    }
    if (project->headers)
    {
        for (uint64_t i = 0; i != array_size(project->headers); i++)
        {
            string_printf(allocator, xml, "        <ClInclude Include=\"%s\"/>\n", path_to_vs_path(project->headers[i]));
        }
    }
    string_printf(allocator, xml, "    </ItemGroup>\n");
    string_printf(allocator, xml, "    <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n");
    string_printf(allocator, xml, "</Project>");
    allocator_destroy(arena_allocator);
    return xml;
}

char* slnx_to_string(SlnFile* sln, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    StringPtrHash h = {.allocator = temp_allocator};
    for (size_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_tolower(p->guid);
        path_backslash_to_slash(p->folder_path);
        bool b_existed;
        uint32_t j = hash_insert_check(&h, p->folder_path, &b_existed);
        Vcxproj** list = NULL;
        if (b_existed)
        {
            list = hash_value(&h, j);
        }
        array_push(temp_allocator, list, p);
        hash_value(&h, j) = (void*)list;
    }
    char* out_str = NULL;
    string_printf(allocator, out_str, "<Solution>\n");
    string_printf(allocator, out_str, "  <Configurations>\n");
    string_printf(allocator, out_str, "    <BuildType Name=\"%s\" />\n", sln->configuration);
    string_printf(allocator, out_str, "    <Platform Name=\"%s\" />\n", sln->architecture);
    string_printf(allocator, out_str, "  </Configurations>\n");
    for (uint32_t i = h.begin; i != h.end; i = hash_next(&h, i))
    {
        Vcxproj** list = hash_value(&h, i);
        char const* folder = hash_key(&h, i);
        if (folder[0])
        {
            string_printf(allocator, out_str, "  <Folder Name=\"/%s/\">\n", folder);
            for (size_t j = 0; j != array_size(list); j++)
            {
                Vcxproj* p = list[j];
                path_backslash_to_slash(p->path);
                string_printf(allocator, out_str, "    <Project Path=\"%s\" Id=\"%s\">\n", p->path, p->guid);
                string_printf(allocator, out_str, "      <BuildType Project=\"%s\" />\n", p->config_name);
                string_printf(allocator, out_str, "      <Platform Project=\"%s\" />\n", p->arch_name);
                string_printf(allocator, out_str, "    </Project>\n");
            }
            string_printf(allocator, out_str, "  </Folder>\n");
        }
        else
        {
            for (size_t j = 0; j != array_size(list); j++)
            {
                Vcxproj* p = list[j];
                path_backslash_to_slash(p->path);
                string_printf(allocator, out_str, "  <Project Path=\"%s\" Id=\"%s\">\n", p->path, p->guid);
                string_printf(allocator, out_str, "    <BuildType Project=\"%s\" />\n", p->config_name);
                string_printf(allocator, out_str, "    <Platform Project=\"%s\" />\n", p->arch_name);
                string_printf(allocator, out_str, "  </Project>\n");
            }
        }
    }
    string_printf(allocator, out_str, "</Solution>\n");
    return out_str;
}

char* sln_to_string(SlnFile* sln, char const* old_guid, Allocator* allocator)
{
    vcxproj_make_sln_folder_hash(sln->projects, sln->hash_folder_to_parent_guid, sln->hash_folder_path_to_guid, sln->allocator);

    Allocator* arena_allocator = allocator_create_chained();
    char* out_str = NULL;

    // Header
    string_printf(allocator, out_str, "Microsoft Visual Studio Solution File, Format Version 12.00\r\n");
    string_printf(allocator, out_str, "# Visual Studio Version 17\r\n");
    string_printf(allocator, out_str, "VisualStudioVersion = 17.0.31912.275\r\n");
    string_printf(allocator, out_str, "MinimumVisualStudioVersion = 10.0.40219.1\r\n");

    // Project section
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_printf(
            allocator,
            out_str,
            "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"{%s}\"\r\n",
            p->name,
            p->path,
            p->guid);
#if 0
        if (p->dependencies && array_size(p->dependencies))
        {
            string_printf(allocator, out_str, "\tProjectSection(ProjectDependencies) = postProject\r\n");
            for (uint64_t j = 0; j != array_size(p->dependencies); j++)
            {
                Vcxproj const* dep = p->dependencies[j];
                string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", dep->guid, dep->guid);
            }
            string_printf(allocator, out_str, "\tEndProjectSection\r\n");
        }
#endif
        string_printf(allocator, out_str, "EndProject\r\n");
    }

    // Folders
    StringPtrHash* h = sln->hash_folder_path_to_guid;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* folder_path = hash_key(h, i);
        char const* stem = path_stem(folder_path, arena_allocator);
        char const* folder_guid = hash_value(h, i);
        string_printf(allocator, out_str, "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"%s\", \"%s\", \"{%s}\"\r\n", stem, stem, folder_guid);
        string_printf(allocator, out_str, "EndProject\r\n");
    }

    string_printf(allocator, out_str, "Global\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n");
    string_printf(allocator, out_str, "\t\t%s|%s = %s|%s\r\n", sln->configuration, sln->architecture, sln->configuration, sln->architecture);
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n");
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        string_printf(allocator, out_str, "\t\t{%s}.%s|%s.ActiveCfg = %s|%s\r\n", p->guid, sln->configuration, sln->architecture, sln->configuration, sln->architecture);
        if (!p->b_exclude_in_build)
        {
            string_printf(allocator, out_str, "\t\t{%s}.%s|%s.Build.0 = %s|%s\r\n", p->guid, sln->configuration, sln->architecture, sln->configuration, sln->architecture);
        }
    }
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");
    string_printf(allocator, out_str, "\tGlobalSection(SolutionProperties) = preSolution\r\n");
    char const* guid = old_guid;
    string_printf(allocator, out_str, "\t\tHideSolutionNode = FALSE\r\n");
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(NestedProjects) = preSolution\r\n");
    for (uint64_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        char const* folder_guid = hash_get(sln->hash_folder_path_to_guid, p->folder_path);
        if (folder_guid)
        {
            string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", p->guid, folder_guid);
        }
    }

    h = sln->hash_folder_to_parent_guid;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        char const* folder = hash_key(h, i);
        char const* parent = hash_value(h, i);
        string_printf(allocator, out_str, "\t\t{%s} = {%s}\r\n", folder, parent);
    }
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");

    string_printf(allocator, out_str, "\tGlobalSection(ExtensibilityGlobals) = postSolution\r\n");
    string_printf(allocator, out_str, "\t\tSolutionGuid = {%s}\r\n", guid);
    string_printf(allocator, out_str, "\tEndGlobalSection\r\n");
    string_printf(allocator, out_str, "EndGlobal\r\n");
    allocator_destroy(arena_allocator);
    return out_str;
}

static Vcxproj* src_project(Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    char const* out_dir = get_var("out_dir");
    char const* cwd = get_var("workspace");
    char* project_path = string_from_print(allocator, "%s\\ProjectFiles\\source.vcxproj", out_dir);
    char* guid = vcxproj_find_or_create_guid(project_path, temp_allocator);
    char const* cfg_name = get_optimization_string(default_optimization_type);
    Vcxproj* p = vcxproj_create(
        allocator,
        "source",
        project_path,
        guid,
        cfg_name,
        get_arch_string(default_architecture_type),
        NULL,
        "",
        NULL,
        NULL);
    Node** nodes = get_all_nodes();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Node* node = nodes[i];
        if (node->node_type != NODE_TYPE_FILE || node->file_type != FILE_TYPE_OBJ)
        {
            continue;
        }
        CCompileCmd* cc = obj_get_compile_cmd(node);
        if (cc == NULL)
        {
            continue;
        }
        char const* src_path = cc->src->path;
        if (path_is_absolute(src_path) && !path_is_under_directory(src_path, cwd))
        {
            continue;
        }
        vcxproj_add_source(p, src_path);
        char** includes = obj_get_compile_includes(node, temp_allocator);
        for (size_t j = 0; j != array_size(includes); j++)
        {
            char const* inc = includes[j];
            vcxproj_add_include_dir_for_source(p, src_path, inc);
        }
    }
    return p;
}

static SlnFile* sln_from_graph(Allocator* allocator)
{
    Hash hash_node_to_vcxproj = {.allocator = allocator};
    SlnFile* sln = allocator_calloc(allocator, 1, sizeof(SlnFile));
    sln->allocator = allocator;
    sln->hash_folder_path_to_guid = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    sln->hash_folder_path_to_guid->allocator = allocator;
    sln->hash_folder_to_parent_guid = allocator_calloc(allocator, 1, sizeof(StringPtrHash));
    sln->hash_folder_to_parent_guid->allocator = allocator;
    Node** nodes = get_all_nodes();
    for (size_t i = 0; i != array_size(nodes); i++)
    {
        Vcxproj* p = target_vcxproj_from_node(nodes[i], allocator);
        if (p)
        {
            sln_add_project(sln, p);
            uint64_t key = (uintptr_t)nodes[i];
            hash_put(&hash_node_to_vcxproj, key, (uintptr_t)p);
        }
    }
    Vcxproj* src_proj = src_project(allocator);
    sln_add_project(sln, src_proj);
    Hash* h = &hash_node_to_vcxproj;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Node* node = (Node*)(uintptr_t)hash_key(h, i);
        Vcxproj* p = (Vcxproj*)(uintptr_t)hash_value(h, i);
        vcxproj_set_deps(p, node, h);
    }
    return sln;
}

static char const** vcxproj_get_files(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    char const** items[] = {project->sources, project->headers, NULL};
    char const** result = NULL;
    for (uint64_t j = 0; items[j]; j++)
    {
        char const** files = items[j];
        for (uint64_t i = 0; i != array_size(files); i++)
        {
            array_push(allocator, result, files[i]);
        }
    }
    allocator_destroy(arena_allocator);
    return result;
}

static int vcxproj_find_paths_common(char const** paths)
{
    int common_index = 0;
    uint64_t num_paths = array_size(paths);
    while (true)
    {
        uint64_t i = num_paths;
        char c = paths[0][common_index];
        do
        {
            if (paths[i - 1][common_index] != c)
            {
                break;
            }
            i--;
        } while (i && c);
        if (i != 0 || c == 0)
        {
            break;
        }
        ++common_index;
    }
    while (common_index != 0 && paths[0][common_index - 1] != '\\')
    {
        common_index--;
    }
    return common_index;
}

static VcxprojFilter* vcxproj_add_parent_filters(StringPtrHash* filters, char const* path, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    path = string_from_c_str(temp_allocator, path);
    VcxprojFilter* first = NULL;
    for (;;)
    {
        char* parent = path_parent_path(path, temp_allocator);
        if (parent[0] == 0)
        {
            break;
        }
        array_free(temp_allocator, path);
        path_slash_to_backslash(parent);
        path = parent;
        bool b_existed;
        uint32_t i = hash_insert_check(filters, path, &b_existed);
        VcxprojFilter* filter;
        if (!b_existed)
        {
            filter = allocator_malloc(allocator, sizeof(VcxprojFilter));
            filter->guid = os_create_guid(allocator, true);
            filter->path = string_from_c_str(allocator, path);
            filter->name = string_from_c_str(allocator, path);
            hash_value(filters, i) = filter;
            hash_key(filters, i) = filter->path;
        }
        else
        {
            filter = (VcxprojFilter*)hash_value(filters, i);
        }
        if (!first)
        {
            first = filter;
        }
    }
    allocator_destroy(temp_allocator);
    return first;
}

static VcxprojFilter** vcxproj_find_filters(Vcxproj const* project, StringPtrHash* hash_path_to_filter, Allocator* allocator)
{
    Allocator* temp_allocator = allocator_create_chained();
    StringPtrHash filters = {.allocator = temp_allocator};
    char const** files = vcxproj_get_files(project, allocator);

    VcxprojFilter** result = NULL;
    if (array_size(files) == 0)
    {
        allocator_destroy(temp_allocator);
        return result;
    }
    for (uint64_t i = 0; i != array_size(files); i++)
    {
        char const* p = files[i];
        VcxprojFilter* f = vcxproj_add_parent_filters(&filters, p, allocator);
        char const* key = files[i];
        hash_put(hash_path_to_filter, key, (void*)f);
    }
    for (uint32_t i = filters.begin; i != filters.end; i = hash_next(&filters, i))
    {
        VcxprojFilter* filter = (VcxprojFilter*)hash_value(&filters, i);
        array_push(allocator, result, filter);
    }
    allocator_destroy(temp_allocator);
    return result;
}

char* vcxproj_filters_to_string(Vcxproj const* project, Allocator* allocator)
{
    Allocator* arena_allocator = allocator_create_chained();
    StringPtrHash hash_path_to_filter = {.allocator = arena_allocator};
    VcxprojFilter** filters = vcxproj_find_filters(project, &hash_path_to_filter, arena_allocator);
    char* out = NULL;
    string_printf(allocator, out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    string_printf(allocator, out, "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    for (uint64_t i = 0; i != array_size(filters); i++)
    {
        VcxprojFilter* f = filters[i];
        string_printf(allocator, out, "        <Filter Include=\"%s\" />\n", f->name);
    }
    string_printf(allocator, out, "    </ItemGroup>\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    if (project->sources)
    {
        for (uint64_t i = 0; i != array_size(project->sources); i++)
        {
            char const* file = project->sources[i];
            VcxprojFilter* filter = (VcxprojFilter*)hash_get(&hash_path_to_filter, file);
            if (!filter)
            {
                continue;
            }
            string_printf(allocator, out, "        <ClCompile Include=\"%s\">\n", path_to_vs_path(file));
            string_printf(allocator, out, "            <Filter>%s</Filter>\n", filter->name);
            string_printf(allocator, out, "        </ClCompile>\n");
        }
    }
    string_printf(allocator, out, "    </ItemGroup>\n");

    string_printf(allocator, out, "    <ItemGroup>\n");
    if (project->headers)
    {
        for (uint64_t i = 0; i != array_size(project->headers); i++)
        {
            char const* file = project->headers[i];
            VcxprojFilter* filter = (VcxprojFilter*)hash_get(&hash_path_to_filter, file);
            if (!filter)
            {
                continue;
            }
            string_printf(allocator, out, "        <ClInclude Include=\"%s\">\n", path_to_vs_path(file));
            string_printf(allocator, out, "            <Filter>%s</Filter>\n", filter->name);
            string_printf(allocator, out, "        </ClInclude>\n");
        }
    }
    string_printf(allocator, out, "    </ItemGroup>\n");
    string_printf(allocator, out, "</Project>\n");

    allocator_destroy(arena_allocator);
    return out;
}

char* vcxproj_user_file_to_string(Vcxproj const* project, Allocator* allocator)
{
    char* content = NULL;
    string_printf(allocator, content, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
    string_printf(allocator, content, "<Project ToolsVersion=\"Current\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n");
    string_printf(allocator, content, "    <PropertyGroup>\r\n");
    string_printf(allocator, content, "        <LocalDebuggerWorkingDirectory>%s</LocalDebuggerWorkingDirectory>\r\n", project->debugger_working_dir);
    string_printf(allocator, content, "        <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>\r\n");
    string_printf(allocator, content, "        <LocalDebuggerCommand>$(SolutionDir)%s</LocalDebuggerCommand>\r\n", project->target_path);
    if (project->debugger_args)
    {
        string_printf(allocator, content, "        <LocalDebuggerCommandArguments>");
        for (size_t i = 0; i != array_size(project->debugger_args); i++)
        {
            char const* arg = project->debugger_args[i];
            if (i != 0)
            {
                string_printf(allocator, content, " ");
            }
            string_printf(allocator, content, "%s", arg);
        }
        string_printf(allocator, content, "</LocalDebuggerCommandArguments>\r\n");
    }
    string_printf(allocator, content, "    </PropertyGroup>\r\n");
    string_printf(allocator, content, "</Project>");
    return content;
}

bool b_generate_vs_projects = false;

void set_generate_vs_projects_enabled(bool enabled)
{
    b_generate_vs_projects = enabled;
}

static int gen_vs_projects_fn(Node* cmd)
{
    char const* sln_path = cmd->outputs[0]->path;
    Allocator* allocator = allocator_create_chained();
    SlnFile* sln = sln_from_graph(allocator);
    for (size_t i = 0; i != array_size(sln->projects); i++)
    {
        Vcxproj* p = sln->projects[i];
        char const* new_content = vcxproj_to_string(p, allocator);
        char const* old_content = os_read_all(allocator, p->path);
        if (!old_content || !string_equal(old_content, new_content))
        {
            os_ensure_dir_existed(p->path);
            os_write_all(p->path, new_content, array_size(new_content));
        }
        array_free(allocator, old_content);
        array_free(allocator, new_content);
        if (p->sources)
        {
            char const* filters_path = string_from_print(allocator, "%s.filters", p->path);
            char* filters = vcxproj_filters_to_string(p, allocator);
            os_ensure_dir_existed(filters_path);
            os_write_all(filters_path, filters, array_size(filters));
        }
        if (p->b_executable)
        {
            char const* vcxproj_user = vcxproj_user_file_to_string(p, allocator);
            char const* user_file_path = string_from_print(allocator, "%s.user", p->path);
            char const* old_user_setting = os_read_all(allocator, user_file_path);
            if (!old_user_setting || !string_equal(vcxproj_user, old_user_setting))
            {
                os_ensure_dir_existed(p->path);
                os_write_all(user_file_path, vcxproj_user, array_size(vcxproj_user));
            }
            array_free(allocator, old_user_setting);
            array_free(allocator, vcxproj_user);
        }
    }
    char const* sln_content;
    if (string_equal(vs_version, "vs2022"))
    {
        sln_content = sln_to_string(sln, sln_find_or_create_guid(sln_path, allocator), allocator);
    }
    else if (string_equal(vs_version, "vs2026"))
    {
        sln_content = slnx_to_string(sln, allocator);
    }
    else
    {
        goto Failed;
    }
    os_ensure_dir_existed(sln_path);
    os_write_all(sln_path, sln_content, array_size(sln_content));
    allocator_destroy(allocator);
    return EXIT_SUCCESS;
Failed:
    allocator_destroy(allocator);
    return EXIT_FAILURE;
}

static bool gen_vs_projects_check_dirty(Node* cmd)
{
    return true;
}

void set_vs_project_version(char const* version)
{
    vs_version = version;
    if (string_equal(version, "vs2026"))
    {
        platform_toolset = "v145";
    }
    else if (string_equal(version, "vs2022"))
    {
        platform_toolset = "v143";
    }
}

ENTRY(gen_vs_projects)
{
    Node* cmd = CALLBACK_CMD(gen_vs_projects_fn, NULL);
    cmd->b_default_excluded = !b_generate_vs_projects;
    node_set_alias(cmd, "vsproj");
    node_set_check_dirty_fn(cmd, gen_vs_projects_check_dirty);
    Allocator* allocator = allocator_temp();
    char const* cwd = get_var("workspace");
    char const* project_name = path_stem(cwd, allocator);
    char* sln_path = string_from_print(allocator, "%s.slnx", project_name);
    Node* sln = get_or_add_file(sln_path);
    sln->b_default_excluded = !b_generate_vs_projects;
    cmd_add_output(cmd, sln);
    cmd_set_description(cmd, fmt("{color_exe}Generating{#} {color_out}{:n}{#}", sln));
}