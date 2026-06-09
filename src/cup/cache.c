#include "cup/cache.h"
#include "core/macros.h"
#include "core/os.h"
#include "core/string.h"
#include "cup/fmt.h"

Cache* cache = NULL;

static const uint32_t CACHE_MAGIC = 0x43505543; // "CUPC" (Little Endian)
static const uint32_t CACHE_VERSION = 2;

#define cache_init_hash(var)                                  \
    (var) = allocator_calloc(allocator, 1, sizeof((var)[0])); \
    (var)->allocator = allocator

Cache* cache_create(Allocator* allocator)
{
    Cache* c = allocator_calloc(allocator, 1, sizeof(Cache));
    c->allocator = allocator;
    cache_init_hash(c->hash_string_to_id);
    cache_init_hash(c->hash_path_to_input_file_record);
    cache_init_hash(c->hash_path_to_output_file_record);
    cache_init_hash(c->hash_name_to_cmd_record);
    cache_init_hash(c->hash_source_path_to_cpp_module_record);
    cache_init_hash(c->hash_source_path_to_test_exe_record);
    return c;
}

#undef cache_init_hash

static bool cache_read_u32(FILE* f, uint32_t* v)
{
    return fread(v, sizeof(uint32_t), 1, f) == 1;
}

static bool cache_file_remaining(FILE* f, uint64_t* out_size)
{
    long pos = ftell(f);
    if (pos < 0) return false;
    if (fseek(f, 0, SEEK_END) != 0) return false;
    long end = ftell(f);
    if (fseek(f, pos, SEEK_SET) != 0) return false;
    if (end < pos) return false;
    *out_size = (uint64_t)(end - pos);
    return true;
}

static bool cache_file_has_remaining(FILE* f, uint64_t size)
{
    uint64_t remaining = 0;
    if (!cache_file_remaining(f, &remaining)) return false;
    return remaining >= size;
}

static bool cache_read_bytes(FILE* f, void* data, size_t size)
{
    if (size == 0) return true;
    if (!cache_file_has_remaining(f, size)) return false;
    return fread(data, 1, size, f) == size;
}

static bool cache_read_str(FILE* f, Allocator* allocator, char** out_str)
{
    uint32_t len = 0;
    if (!cache_read_u32(f, &len)) return false;
    if (!cache_file_has_remaining(f, len)) return false;
    char* str = string_new(allocator, len, NULL);
    if (len > 0)
    {
        if (!cache_read_bytes(f, str, len))
        {
            string_free(allocator, str);
            return false;
        }
    }
    *out_str = str;
    return true;
}

static bool cache_write_u32(FILE* f, uint32_t v)
{
    return fwrite(&v, sizeof(uint32_t), 1, f) == 1;
}

static bool cache_write_str(FILE* f, char const* str)
{
    uint32_t len = (uint32_t)(str ? strlen(str) : 0);
    if (!cache_write_u32(f, len)) return false;
    if (len > 0)
    {
        return fwrite(str, 1, len, f) == len;
    }
    return true;
}

void init_cache(void)
{
    if (cache == NULL)
    {
        char const* path = fmt("{out_dir}/.cup_cache");
        cache = cache_load(allocator_c(), path);
    }
}

Cache* get_cache(void)
{
    return cache;
}

size_t cache_calc_num_records(Cache* c)
{
    size_t num_records = 0;
    num_records += c->hash_path_to_input_file_record->size;
    num_records += c->hash_path_to_output_file_record->size;
    num_records += c->hash_name_to_cmd_record->size;
    num_records += c->hash_source_path_to_cpp_module_record->size;
    num_records += c->hash_source_path_to_test_exe_record->size;
    return num_records;
}

Cache* cache_load_readonly(Allocator* allocator, char const* path)
{
    Cache* c = cache_create(allocator);
    FILE* f = os_fopen(path, "r+b");
    bool b_invalid_cache = false;
    if (!f)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    uint32_t magic = 0, version = 0;
    if (!cache_read_u32(f, &magic) || magic != CACHE_MAGIC)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    if (!cache_read_u32(f, &version) || version != CACHE_VERSION)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    c->log_file = f;
    if (!cache_read_records(c))
    {
        b_invalid_cache = true;
    }
    else
    {
        c->num_valid_records = cache_calc_num_records(c);
    }
EndLoad:
    if (f)
    {
        fclose(f);
    }
    c->log_file = NULL;
    if (b_invalid_cache)
    {
        cache_destroy(c);
        c = cache_create(allocator);
    }
    return c;
}

void cache_clear(Cache* c);

Cache* cache_load(Allocator* allocator, char const* path)
{
    os_ensure_dir_existed(path);
    Cache* c = cache_create(allocator);
    bool b_invalid_cache = false;
    FILE* f = os_fopen(path, "r+b");
    if (!f)
    {
        goto OpenLog;
    }
    uint32_t magic = 0, version = 0;
    if (!cache_read_u32(f, &magic) || magic != CACHE_MAGIC)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    if (!cache_read_u32(f, &version) || version != CACHE_VERSION)
    {
        b_invalid_cache = true;
        goto EndLoad;
    }
    c->log_file = f;
    if (!cache_read_records(c))
    {
        b_invalid_cache = true;
    }
    else
    {
        c->num_valid_records = cache_calc_num_records(c);
    }
EndLoad:
    if (f)
    {
        fclose(f);
    }
    c->log_file = NULL;
    if (b_invalid_cache)
    {
        error("Invalid cache, deleting: %s", path);
        cache_clear(c);
        allocator_free(allocator, c);
        os_remove_file(path);
        c = cache_create(allocator);
    }
OpenLog:
    if (c->total_records_read > c->num_valid_records * 2)
    {
        c->b_needs_compaction = true;
    }
    c->log_file = os_fopen(path, "ab");
    expect(c->log_file, "cache log file is NULL");
    fseek(c->log_file, 0, SEEK_END);
    if (c->log_file && ftell(c->log_file) == 0)
    {
        cache_write_u32(c->log_file, CACHE_MAGIC);
        cache_write_u32(c->log_file, CACHE_VERSION);
        fflush(c->log_file);
    }
    return c;
}

static void cache_destroy_record_test_exe(Allocator* allocator, CacheRecordTestExe* record)
{
    for (size_t i = 0; i != array_size(record->entries); i++)
    {
        char* entry = record->entries[i];
        array_free(allocator, entry);
    }
    allocator_free(allocator, record);
}

static void cache_destroy_record_cpp_module(Allocator* allocator, CacheRecordCppModule* record)
{
    for (size_t i = 0; i != array_size(record->imports); i++)
    {
        char* name = record->imports[i];
        array_free(allocator, name);
    }
    array_free(allocator, record->export);
    allocator_free(allocator, record);
}

static void cache_destroy_record_cmd(Allocator* allocator, CacheRecordCmd* record)
{
    array_free(allocator, record->implicit_inputs);
    array_free(allocator, record->outputs);
    array_free(allocator, record->inputs);
    string_free(allocator, record->cmdline);
    string_free(allocator, record->name);
}

void cache_destroy_files(Cache* c)
{
    StringPtrHash* h;
    h = c->hash_path_to_output_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        allocator_free(c->allocator, record);
    }
    hash_free(c->hash_path_to_output_file_record);
    h = c->hash_path_to_input_file_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordFile* record = hash_value(h, i);
        allocator_free(c->allocator, record);
    }
    hash_free(c->hash_path_to_input_file_record);
    for (size_t i = 0; i != array_size(c->strings); i++)
    {
        char* path = c->strings[i];
        string_free(c->allocator, path);
    }
    hash_free(c->hash_string_to_id);
    array_free(c->allocator, c->strings);
}

void cache_clear(Cache* c)
{
    if (c->log_file)
    {
        fclose(c->log_file);
    }

    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordTestExe* record = hash_value(h, i);
        cache_destroy_record_test_exe(c->allocator, record);
    }
    hash_free(c->hash_source_path_to_test_exe_record);
    h = c->hash_source_path_to_cpp_module_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCppModule* record = hash_value(h, i);
        cache_destroy_record_cpp_module(c->allocator, record);
    }
    hash_free(c->hash_source_path_to_cpp_module_record);

    h = c->hash_name_to_cmd_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCmd* record = hash_value(h, i);
        cache_destroy_record_cmd(c->allocator, record);
    }
    hash_free(c->hash_name_to_cmd_record);
    cache_destroy_files(c);
}

void cache_destroy(Cache* c)
{
    cache_clear(c);
    allocator_free(c->allocator, c);
}

static CacheRecordFile* cache_merge_file_record(Cache* c, char const* path, StringPtrHash* h)
{
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    if (b_existed)
    {
        return hash_value(h, i);
    }
    CacheRecordFile* record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordFile));
    record->id = cache_get_or_insert_string(c, path);
    hash_key(h, i) = c->strings[record->id];
    hash_value(h, i) = record;
    return record;
}

CacheRecordFile* cache_merge_in_file_record(Cache* c, char const* path)
{
    return cache_merge_file_record(c, path, c->hash_path_to_input_file_record);
}

CacheRecordFile* cache_merge_out_file_record(Cache* c, char const* path)
{
    return cache_merge_file_record(c, path, c->hash_path_to_output_file_record);
}

static void cache_copy_cmd_record(Allocator* allocator, CacheRecordCmd* dst, CacheRecordCmd const* src)
{
    if (dst == src)
    {
        return;
    }
    string_clear(dst->name);
    string_concat_c_str(allocator, dst->name, src->name);
    string_clear(dst->cmdline);
    string_concat_c_str(allocator, dst->cmdline, src->cmdline);
    size_t num_inputs = array_size(src->inputs);
    array_resize(allocator, dst->inputs, 0);
    array_push_v(allocator, dst->inputs, src->inputs, num_inputs);
    size_t num_outputs = array_size(src->outputs);
    array_resize(allocator, dst->outputs, 0);
    array_push_v(allocator, dst->outputs, src->outputs, num_outputs);
    size_t num_implicit_inputs = array_size(src->implicit_inputs);
    array_resize(allocator, dst->implicit_inputs, 0);
    array_push_v(allocator, dst->implicit_inputs, src->implicit_inputs, num_implicit_inputs);
}

void cache_merge_cmd_record(Cache* c, CacheRecordCmd const* cmd)
{
    StringPtrHash* h = c->hash_name_to_cmd_record;
    uint32_t i = hash_index(h, cmd->name);
    if (i != UINT32_MAX)
    {
        CacheRecordCmd* record = hash_value(h, i);
        cache_copy_cmd_record(c->allocator, record, cmd);
        hash_key(h, i) = record->name;
        return;
    }

    CacheRecordCmd* record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordCmd));
    cache_copy_cmd_record(c->allocator, record, cmd);
    bool b_existed = false;
    i = hash_insert_check(h, record->name, &b_existed);
    expect(!b_existed, "cache record already existed");
    hash_key(h, i) = record->name;
    hash_value(h, i) = record;
}

static void cache_copy_cpp_module_record(Allocator* allocator, CacheRecordCppModule* dst, CacheRecordCppModule const* src)
{
    if (dst == src)
    {
        return;
    }
    dst->source_id = src->source_id;
    string_clear(dst->export);
    string_concat_c_str(allocator, dst->export, src->export);
    size_t old_size = array_size(dst->imports);
    size_t new_size = array_size(src->imports);
    for (size_t i = 0; i != new_size; i++)
    {
        char* import_name = src->imports[i];
        if (i >= old_size)
        {
            char* copied = string_from_c_str(allocator, import_name);
            array_push(allocator, dst->imports, copied);
        }
        else
        {
            string_clear(dst->imports[i]);
            string_concat_c_str(allocator, dst->imports[i], import_name);
        }
    }
    if (old_size > new_size)
    {
        for (size_t i = new_size; i != old_size; i++)
        {
            array_free(allocator, dst->imports[i]);
        }
    }
    array_resize(allocator, dst->imports, new_size);
}

void cache_merge_cpp_module_record(Cache* c, CacheRecordCppModule const* module)
{
    StringPtrHash* h = c->hash_source_path_to_cpp_module_record;
    char const* path = cache_get_string(c, module->source_id);
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    CacheRecordCppModule* record = hash_value(h, i);
    if (!b_existed)
    {
        record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordCppModule));
        hash_value(h, i) = record;
    }
    cache_copy_cpp_module_record(c->allocator, record, module);
}

static void cache_copy_test_exe_record(Allocator* allocator, CacheRecordTestExe* dst, CacheRecordTestExe const* src)
{
    if (dst == src)
    {
        return;
    }
    dst->source_id = src->source_id;
    size_t old_size = array_size(dst->entries);
    size_t new_size = array_size(src->entries);
    for (size_t i = 0; i != array_size(src->entries); i++)
    {
        char* name = src->entries[i];
        if (i >= old_size)
        {
            char* copied = string_from_c_str(allocator, name);
            array_push(allocator, dst->entries, copied);
        }
        else
        {
            string_clear(dst->entries[i]);
            string_concat_c_str(allocator, dst->entries[i], name);
        }
    }
    if (old_size > new_size)
    {
        for (size_t i = new_size; i != old_size; i++)
        {
            array_free(allocator, dst->entries[i]);
        }
    }
    array_resize(allocator, dst->entries, new_size);
}

void cache_merge_test_exe_record(Cache* c, CacheRecordTestExe const* exe)
{
    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    char const* path = cache_get_string(c, exe->source_id);
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    CacheRecordTestExe* record = hash_value(h, i);
    if (!b_existed)
    {
        record = allocator_calloc(c->allocator, 1, sizeof(CacheRecordTestExe));
        hash_value(h, i) = record;
    }
    cache_copy_test_exe_record(c->allocator, record, exe);
}

CacheRecordFile* cache_find_file_record(Cache* c, char const* path, StringPtrHash* h)
{
    uint32_t i = hash_index(h, path);
    if (i == HASH_INVALID_INDEX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

char* cache_get_string(Cache* c, uint32_t id)
{
    return c->strings[id];
}

uint32_t cache_get_or_insert_string(Cache* c, char const* path)
{
    StringHash* h = c->hash_string_to_id;
    bool b_existed;
    uint32_t i = hash_insert_check(h, path, &b_existed);
    if (!b_existed)
    {
        hash_value(h, i) = array_size(c->strings);
        char* new_path = string_from_c_str(c->allocator, path);
        hash_key(h, i) = new_path;
        array_push(c->allocator, c->strings, new_path);
    }
    return hash_value(h, i);
}

CacheRecordFile* cache_find_in_file_record(Cache* c, char const* path)
{
    return cache_find_file_record(c, path, c->hash_path_to_input_file_record);
}

CacheRecordFile* cache_find_out_file_record(Cache* c, char const* path)
{
    return cache_find_file_record(c, path, c->hash_path_to_output_file_record);
}

CacheRecordCppModule* cache_find_cpp_module_record(Cache* c, char const* source_path)
{
    StringPtrHash* h = c->hash_source_path_to_cpp_module_record;
    uint32_t i = hash_index(h, source_path);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordTestExe* cache_find_test_exe(Cache* c, char const* source_path)
{
    StringPtrHash* h = c->hash_source_path_to_test_exe_record;
    uint32_t i = hash_index(h, source_path);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordCmd* cache_find_cmd_record(Cache* c, char const* name)
{
    StringPtrHash* h = c->hash_name_to_cmd_record;
    uint32_t i = hash_index(h, name);
    if (i == UINT32_MAX)
    {
        return NULL;
    }
    return hash_value(h, i);
}

CacheRecordFile* cache_add_in_file_record(Cache* c, char const* path)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_IN_FILE;
    cache_write_u32(f, type);
    cache_write_str(f, path);
    return cache_merge_in_file_record(c, path);
}

CacheRecordFile* cache_get_or_add_in_file_record(Cache* c, char const* path)
{
    CacheRecordFile* record = cache_find_in_file_record(c, path);
    if (!record)
    {
        record = cache_add_in_file_record(c, path);
    }
    return record;
}

CacheRecordFile* cache_get_or_add_out_file_record(Cache* c, char const* path)
{
    CacheRecordFile* record = cache_find_out_file_record(c, path);
    if (!record)
    {
        record = cache_add_out_file_record(c, path);
    }
    return record;
}

bool cache_read_in_file_record(Cache* c)
{
    char* path = NULL;
    if (!cache_read_str(c->log_file, c->allocator, &path)) return false;
    cache_merge_in_file_record(c, path);
    string_free(c->allocator, path);
    return true;
}

CacheRecordFile* cache_add_out_file_record(Cache* c, char const* path)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_OUT_FILE;
    cache_write_u32(f, type);
    cache_write_str(f, path);
    return cache_merge_out_file_record(c, path);
}

bool cache_read_out_file_record(Cache* c)
{
    char* path = NULL;
    if (!cache_read_str(c->log_file, c->allocator, &path)) return false;
    cache_merge_out_file_record(c, path);
    string_free(c->allocator, path);
    return true;
}

static void cache_write_file_array(Cache* c, CacheFile* files)
{
    uint32_t num_files = array_size(files);
    cache_write_u32(c->log_file, num_files);
    if (num_files)
    {
        fwrite(files, sizeof(CacheFile), num_files, c->log_file);
    }
}

void cache_write_cmd_record(Cache* c, CacheRecordCmd const* cmd)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_CMD;
    cache_write_u32(f, type);
    cache_write_str(f, cmd->name);
    cache_write_str(f, cmd->cmdline);
    cache_write_file_array(c, cmd->inputs);
    cache_write_file_array(c, cmd->outputs);
    cache_write_file_array(c, cmd->implicit_inputs);
    cache_merge_cmd_record(c, cmd);
}

static bool cache_read_file_array(Cache* c, CacheFile** out_files)
{
    FILE* f = c->log_file;
    uint32_t num_files = 0;
    if (!cache_read_u32(f, &num_files)) return false;
    CacheFile* files = NULL;
    if (num_files)
    {
        size_t num_bytes = sizeof(CacheFile) * num_files;
        if (num_bytes / sizeof(CacheFile) != num_files) return false;
        if (!cache_file_has_remaining(f, num_bytes)) return false;
        files = array_new(c->allocator, CacheFile, num_files, 0);
        if (!cache_read_bytes(f, files, num_bytes))
        {
            array_free(c->allocator, files);
            return false;
        }
    }
    *out_files = files;
    return true;
}

static bool cache_validate_file_array(Cache* c, CacheFile* files)
{
    size_t num_cached_files = array_size(c->strings);
    for (size_t i = 0; i != array_size(files); i++)
    {
        if (files[i].id >= num_cached_files) return false;
    }
    return true;
}

bool cache_read_cmd_record(Cache* c)
{
    FILE* f = c->log_file;
    CacheRecordCmd record = {0};
    if (!cache_read_str(f, c->allocator, &record.name)) return false;
    if (!cache_read_str(f, c->allocator, &record.cmdline)) return false;
    if (!cache_read_file_array(c, &record.inputs)) return false;
    if (!cache_read_file_array(c, &record.outputs)) return false;
    if (!cache_read_file_array(c, &record.implicit_inputs)) return false;
    if (!cache_validate_file_array(c, record.inputs)) return false;
    if (!cache_validate_file_array(c, record.outputs)) return false;
    if (!cache_validate_file_array(c, record.implicit_inputs)) return false;
    cache_merge_cmd_record(c, &record);
    string_free(c->allocator, record.name);
    string_free(c->allocator, record.cmdline);
    array_free(c->allocator, record.inputs);
    array_free(c->allocator, record.outputs);
    array_free(c->allocator, record.implicit_inputs);
    return true;
}

static void cache_write_string_array(Cache* c, char** strings)
{
    FILE* f = c->log_file;
    size_t num_string = array_size(strings);
    cache_write_u32(f, num_string);
    for (size_t i = 0; i != num_string; i++)
    {
        char const* string = strings[i];
        cache_write_str(f, string);
    }
}

static bool cache_read_string_array(Cache* c, char*** out_strings)
{
    uint32_t array_size = 0;
    if (!cache_read_u32(c->log_file, &array_size)) return false;
    if (array_size == 0)
    {
        *out_strings = NULL;
        return true;
    }
    size_t min_bytes = sizeof(uint32_t) * array_size;
    if (min_bytes / sizeof(uint32_t) != array_size) return false;
    if (!cache_file_has_remaining(c->log_file, min_bytes)) return false;
    char** strings = array_new(c->allocator, char*, array_size, 0);
    for (size_t i = 0; i != array_size; i++)
    {
        if (!cache_read_str(c->log_file, c->allocator, &strings[i]))
        {
            for (size_t j = 0; j != i; j++)
            {
                string_free(c->allocator, strings[j]);
            }
            array_free(c->allocator, strings);
            return false;
        }
    }
    *out_strings = strings;
    return true;
}

void cache_write_cpp_module_record(Cache* c, CacheRecordCppModule const* module)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_CPP_MODULE;
    cache_write_u32(f, type);
    fwrite(&module->source_id, sizeof(module->source_id), 1, f);
    cache_write_str(f, module->export);
    cache_write_string_array(c, module->imports);
    cache_merge_cpp_module_record(c, module);
}

bool cache_read_cpp_module_record(Cache* c)
{
    CacheRecordCppModule record = {0};
    if (!cache_read_bytes(c->log_file, &record.source_id, sizeof(record.source_id))) return false;
    if (record.source_id >= array_size(c->strings)) return false;
    if (!cache_read_str(c->log_file, c->allocator, &record.export)) return false;
    if (!cache_read_string_array(c, &record.imports)) return false;
    cache_merge_cpp_module_record(c, &record);
    array_free(c->allocator, record.export);
    for (size_t i = 0; i != array_size(record.imports); i++)
    {
        string_free(c->allocator, record.imports[i]);
    }
    array_free(c->allocator, record.imports);
    return true;
}

void cache_write_test_exe_record(Cache* c, CacheRecordTestExe const* exe)
{
    FILE* f = c->log_file;
    uint32_t type = CACHE_RECORD_TYPE_TEST_EXE;
    cache_write_u32(f, type);
    fwrite(&exe->source_id, sizeof(exe->source_id), 1, f);
    cache_write_string_array(c, exe->entries);
    cache_merge_test_exe_record(c, exe);
}

bool cache_read_test_exe_record(Cache* c)
{
    CacheRecordTestExe record = {0};
    if (!cache_read_bytes(c->log_file, &record.source_id, sizeof(record.source_id))) return false;
    if (record.source_id >= array_size(c->strings)) return false;
    if (!cache_read_string_array(c, &record.entries)) return false;
    cache_merge_test_exe_record(c, &record);
    for (size_t i = 0; i != array_size(record.entries); i++)
    {
        string_free(c->allocator, record.entries[i]);
    }
    array_free(c->allocator, record.entries);
    return true;
}

bool cache_read_records(Cache* c)
{
    uint32_t type = CACHE_RECORD_TYPE_EMPTY;
    while (true)
    {
        uint64_t remaining = 0;
        if (!cache_file_remaining(c->log_file, &remaining)) return false;
        if (remaining == 0)
        {
            return true;
        }
        if (remaining < sizeof(type)) return false;
        if (!cache_read_u32(c->log_file, &type)) return false;
        if (type == CACHE_RECORD_TYPE_EMPTY) return true;
        c->total_records_read++;
        bool b_ok = false;
        switch (type)
        {
        case CACHE_RECORD_TYPE_IN_FILE: b_ok = cache_read_in_file_record(c); break;
        case CACHE_RECORD_TYPE_OUT_FILE: b_ok = cache_read_out_file_record(c); break;
        case CACHE_RECORD_TYPE_CMD: b_ok = cache_read_cmd_record(c); break;
        case CACHE_RECORD_TYPE_CPP_MODULE: b_ok = cache_read_cpp_module_record(c); break;
        case CACHE_RECORD_TYPE_TEST_EXE: b_ok = cache_read_test_exe_record(c); break;
        default:
            error("Unknown cache record type: %d", type);
            return false;
        }
        if (!b_ok) return false;
    }
}

void cache_compact_log(Cache* c, char const* path)
{
    if (!c->b_needs_compaction) return;
    if (c->log_file)
    {
        fclose(c->log_file);
        c->log_file = NULL;
    }
    char const* tmp_path = string_from_print(allocator_temp(), "%s.tmp", path);
    FILE* tmpfile = os_fopen(tmp_path, "wb");
    cache_write_u32(tmpfile, CACHE_MAGIC);
    cache_write_u32(tmpfile, CACHE_VERSION);
    c->total_records_read = 0;

    Cache* temp_cache = cache_create(c->allocator);
    temp_cache->log_file = tmpfile;

    StringPtrHash* h = c->hash_name_to_cmd_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCmd* cmd = hash_value(h, i);
        for (size_t j = 0; j != array_size(cmd->inputs); j++)
        {
            uint32_t* id = &cmd->inputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_in_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        for (size_t j = 0; j != array_size(cmd->outputs); j++)
        {
            uint32_t* id = &cmd->outputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_out_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_out_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        for (size_t j = 0; j != array_size(cmd->implicit_inputs); j++)
        {
            uint32_t* id = &cmd->implicit_inputs[j].id;
            char* path = cache_get_string(c, *id);
            CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
            if (!record)
            {
                record = cache_add_in_file_record(temp_cache, path);
            }
            *id = record->id;
        }
        cache_write_cmd_record(temp_cache, cmd);
    }
    h = c->hash_source_path_to_cpp_module_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordCppModule* cmd = hash_value(h, i);
        uint32_t* id = &cmd->source_id;
        char* path = cache_get_string(c, *id);
        CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
        if (!record)
        {
            record = cache_add_in_file_record(temp_cache, path);
        }
        *id = record->id;
        cache_write_cpp_module_record(temp_cache, cmd);
    }
    h = c->hash_source_path_to_test_exe_record;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        CacheRecordTestExe* cmd = hash_value(h, i);
        uint32_t* id = &cmd->source_id;
        char* path = cache_get_string(c, *id);
        CacheRecordFile* record = cache_find_in_file_record(temp_cache, path);
        if (!record)
        {
            record = cache_add_in_file_record(temp_cache, path);
        }
        *id = record->id;
        cache_write_test_exe_record(temp_cache, cmd);
    }
    cache_clear(c);
    *c = *temp_cache;
    allocator_free(c->allocator, temp_cache);

    fclose(tmpfile);
    os_rename(tmp_path, path);
    c->log_file = os_fopen(path, "ab");
    c->b_needs_compaction = false;
}
