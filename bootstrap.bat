@echo off
setlocal

set CLANG_FLAGS=
if "%~1"=="-m32" (
    set CLANG_FLAGS=-m32
)

if not exist build mkdir build

clang src/core/hash_gen.c -g -O0 -Isrc -o build/hash_gen.exe -Wno-deprecated-declarations -Wno-microsoft-anon-tag -Xlinker /incremental:no -Xlinker /pdb:build/hash_gen.exe.pdb -MJ build/cc_hash_gen.json || (
    exit /B 1
)

build\hash_gen.exe -o src/core/hash.h src/core/hash_gen.c || (
    exit /B 1
)

@REM tests/test_graph.c src/cup/test_main.c src/cup/test.c -DBUILD_TEST

clang %CLANG_FLAGS%  build.c src/core/build.c src/cup/build.c src/cup/in_repo.c src/cup/c_toolchain/build.c src/cup/bootstrap.c -g -O0 -Isrc -o cup.exe -Wno-deprecated-declarations -Wno-microsoft-anon-tag -Xlinker /noimplib -Xlinker /incremental:no -Xlinker /pdb:build/cup.exe.pdb -MJ build/cc_cup.json || (
    exit /B 1
)

echo [ > compile_commands.json
type build\cc_hash_gen.json >> compile_commands.json
type build\cc_cup.json >> compile_commands.json
echo ] >> compile_commands.json

powershell -NoProfile -Command "$content = Get-Content -Raw -Path compile_commands.json; $content = $content -replace ',\s*\]', ']'; Set-Content -NoNewline -Path compile_commands.json -Value $content"

echo compile_commands.json generated successfully.
