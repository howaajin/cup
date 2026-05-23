#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct Allocator Allocator;

typedef enum
{
    DEPFILE_ITEM_TARGET,
    DEPFILE_ITEM_NORMAL_DEP,
    DEPFILE_ITEM_ORDER_ONLY_DEP,
    DEPFILE_ITEM_PHONY
} DepfileItemType;

typedef struct
{
    FILE* file;
    int state;
    bool is_phony_rule;
} DepfileParser;

void depfile_parser_init(DepfileParser* parser, FILE* f);
bool depfile_parser_next(DepfileParser* parser, struct Allocator* allocator, char** out_path, DepfileItemType* out_type);
