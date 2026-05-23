#!/bin/bash

set -e

CLANG_FLAGS=""

OS=$(uname -s)
LINK_FLAGS=""

mkdir -p build

gcc src/core/hash_gen.c -g -O0 -Isrc -fms-extensions -Wno-microsoft-anon-tag -o build/hash_gen \
    -Wno-deprecated-declarations

build/hash_gen -o src/core/hash.h src/core/hash_gen.c

gcc $CLANG_FLAGS build.c src/core/build.c src/cup/build.c src/cup/in_repo.c src/cup/c_toolchain/build.c \
    src/cup/bootstrap.c -D_GNU_SOURCE -g -O0 -fms-extensions -Wno-microsoft-anon-tag -Isrc -o cup \
    -Wno-deprecated-declarations $LINK_FLAGS