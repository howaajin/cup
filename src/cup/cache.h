#pragma once

#include "core/hash.h"
#include "core/macros.h"

#include <stdbool.h>

#include <stdint.h>
#include <stdio.h>

typedef struct Node Node;
typedef struct Cache Cache;
typedef struct CacheFile CacheFile;
typedef struct CacheRecordCmd CacheRecordCmd;
typedef struct CacheRecordCppModule CacheRecordCppModule;
typedef struct CacheRecordFile CacheRecordFile;
typedef struct CacheRecordTestExe CacheRecordTestExe;

enum CacheRecordType
{
    CACHE_RECORD_TYPE_EMPTY = 0,
    CACHE_RECORD_TYPE_IN_FILE = ENUM_CODE('I', 'N', 'F', 'E'),
    CACHE_RECORD_TYPE_OUT_FILE = ENUM_CODE('O', 'U', 'T', 'F'),
    CACHE_RECORD_TYPE_CMD = ENUM_CODE('C', 'C', 'M', 'D'),
    CACHE_RECORD_TYPE_CPP_MODULE = ENUM_CODE('C', 'P', 'P', 'M'),
    CACHE_RECORD_TYPE_TEST_EXE = ENUM_CODE('T', 'E', 'X', 'E'),

};

struct CacheRecordFile
{
    uint32_t id;
};

struct CacheRecordCmd
{
    char* name;
    char* cmdline;
    CacheFile* inputs;
    CacheFile* outputs;
    CacheFile* implicit_inputs;
};

struct CacheFile
{
    uint32_t id;
    uint64_t build_time;
    uint64_t content_hash;
};

struct CacheRecordCppModule
{
    uint32_t source_id;
    char* export;
    char** imports;
};

struct CacheRecordTestExe
{
    uint32_t source_id;
    char** entries;
};

struct Cache
{
    Allocator* allocator;
    StringPtrHash* hash_path_to_input_file;
    StringPtrHash* hash_path_to_output_file;
    StringPtrHash* hash_name_to_cmd_record;
    StringPtrHash* hash_source_path_to_cpp_module_record;
    StringPtrHash* hash_source_path_to_test_exe_record;
    char** files;
    StringHash* hash_path_to_file_id;

    FILE* log_file;
    uint32_t total_records_read;
    uint32_t num_valid_records;

    bool b_needs_compaction;
};

Cache* get_cache(void);
Cache* cache_load(Allocator* allocator, char const* path);
Cache* cache_load_readonly(Allocator* allocator, char const* path);
void cache_destroy(Cache* c);
void cache_compact_log(Cache* c, char const* path);
char* cache_get_string(Cache* c, uint32_t id);
uint32_t cache_get_or_insert_string(Cache* c, char const* path);
CacheRecordFile* cache_find_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_find_out_file_record(Cache* c, char const* path);
CacheRecordTestExe* cache_find_test_exe(Cache* c, char const* source_path);
CacheRecordCppModule* cache_find_cpp_module_record(Cache* c, char const* source_path);
CacheRecordCmd* cache_find_cmd_record(Cache* c, char const* name);
CacheRecordFile* cache_add_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_add_out_file_record(Cache* c, char const* path);
CacheRecordFile* cache_get_or_add_in_file_record(Cache* c, char const* path);
CacheRecordFile* cache_get_or_add_out_file_record(Cache* c, char const* path);

void cache_write_cmd_record(Cache* c, CacheRecordCmd const* cmd);
void cache_write_cpp_module_record(Cache* c, CacheRecordCppModule const* module);
void cache_write_test_exe_record(Cache* c, CacheRecordTestExe const* exe);
bool cache_read_in_file_record(Cache* c);
bool cache_read_out_file_record(Cache* c);
bool cache_read_cmd_record(Cache* c);
bool cache_read_cpp_module_record(Cache* c);
bool cache_read_test_exe_record(Cache* c);
bool cache_read_records(Cache* c);
