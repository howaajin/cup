#!/bin/bash

set -e

ARCH=""
CLANG_FLAGS=""
LINK_FLAGS=""

while [ $# -gt 0 ]; do
    case "$1" in
        -arch)
            shift
            ARCH="$1"
            ;;
        *)
            echo "error: unknown option $1"
            exit 1
            ;;
    esac
    shift
done

if [ "$(uname -s)" = "Darwin" ] && [ "$ARCH" = "x86" ]; then
    echo "error: -arch x86 is not supported on macOS (32-bit support removed since macOS 10.15)"
    exit 1
fi

if [ "$ARCH" = "x86" ]; then
    CLANG_FLAGS="-m32"
    LINK_FLAGS="-m32"
elif [ "$ARCH" = "x64" ]; then
    CLANG_FLAGS="-m64"
    LINK_FLAGS="-m64"
elif [ -n "$ARCH" ]; then
    echo "error: only supported x86/x64"
    exit 1
fi

mkdir -p build

cc $CLANG_FLAGS src/core/hash_gen.c -g -O0 -Isrc -fms-extensions -Wno-microsoft-anon-tag -o build/hash_gen \
    -Wno-deprecated-declarations

build/hash_gen -o src/core/hash.h src/core/hash_gen.c

cc $CLANG_FLAGS build.c src/core/build.c src/cup/build.c src/cup/in_repo.c src/cup/c_toolchain/build.c \
    src/cup/bootstrap.c -D_GNU_SOURCE -g -O0 -fms-extensions -Wno-microsoft-anon-tag -Isrc -o cup \
    -Wno-deprecated-declarations $LINK_FLAGS