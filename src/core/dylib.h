#pragma once

#include <stdbool.h>

typedef struct Allocator Allocator;
typedef struct Dylib Dylib;

Dylib* dylib_load(char const* name);
void dylib_unload(Dylib* lib);
void* dylib_get_symbol(Dylib* lib, char const* name);
void* dylib_get_image_base(Dylib* lib);
