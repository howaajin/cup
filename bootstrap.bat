@echo off
setlocal

set ARCH=x64
:parse_cmdline
if "%~1"=="" goto set_param

if /i "%~1"=="-arch" (
    set ARCH=%~2
    shift
)
shift
goto parse_cmdline

:set_param

if /i "%ARCH%"=="x86" (
    set ARCH=x86
) else if /i "%ARCH%"=="x64" (
    set ARCH=x64
) else (
    echo error: only supported x86/x64
    exit /b 1
)

if not exist build mkdir build

REM ---- MSVC preferred ----
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 goto build_msvc

set "VS_INSTALL_DIR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS_INSTALL_DIR=%%i"
)
if defined VS_INSTALL_DIR goto init_msvc_env

REM ---- Clang fallback ----
where clang >nul 2>&1
if %ERRORLEVEL% EQU 0 goto build_clang

echo Error: no C compiler found. Install clang or Visual Studio.
exit /B 1

:init_msvc_env
set "MSVC_VER="
for /f "delims=" %%v in ('dir /b /o-n /ad "%VS_INSTALL_DIR%\VC\Tools\MSVC" 2^>nul') do if not defined MSVC_VER set "MSVC_VER=%%v"
if not defined MSVC_VER (
    echo Error: MSVC toolchain not found under "%VS_INSTALL_DIR%". Install the "Desktop development with C++" workload in Visual Studio.
    exit /B 1
)
set "SDK_ROOT="
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul ^| find "KitsRoot10"') do set "SDK_ROOT=%%b"
if not defined SDK_ROOT (
    echo Error: Windows SDK not found. Install it via Visual Studio Installer ^(or from standalone SDK installer^).
    exit /B 1
)
set "SDK_VER="
for /f "delims=" %%v in ('dir /b /o-n /ad "%SDK_ROOT%\Include" 2^>nul ^| findstr /r "^10\."') do if not defined SDK_VER set "SDK_VER=%%v"
if not defined SDK_VER (
    echo Error: Windows SDK version not found in "%SDK_ROOT%\Include".
    exit /B 1
)
set "MSVC_BIN=%VS_INSTALL_DIR%\VC\Tools\MSVC\%MSVC_VER%"
set "INCLUDE=%SDK_ROOT%\Include\%SDK_VER%\ucrt;%SDK_ROOT%\Include\%SDK_VER%\um;%SDK_ROOT%\Include\%SDK_VER%\shared;%SDK_ROOT%\Include\%SDK_VER%\winrt;%SDK_ROOT%\Include\%SDK_VER%\cppwinrt;%MSVC_BIN%\include;"
set "LIB=%SDK_ROOT%\Lib\%SDK_VER%\um\%ARCH%;%SDK_ROOT%\Lib\%SDK_VER%\ucrt\%ARCH%;%MSVC_BIN%\lib\%ARCH%;"
set "PATH=%MSVC_BIN%\bin\Host%ARCH%\%ARCH%;%SDK_ROOT%\bin\%SDK_VER%\%ARCH%;%PATH%"
goto build_msvc

:build_clang
if /i "%ARCH%"=="x86" (
    set ARCH= -m32
) else (
    set ARCH= -m64
)
set "CC_CLANG_HASH=clang src/core/hash_gen.c%ARCH% -g -O0 -Isrc -o build/hash_gen.exe -Wno-deprecated-declarations -Wno-microsoft-anon-tag -Xlinker /incremental:no -Xlinker /pdb:build/hash_gen.exe.pdb"
set "CC_CLANG_CUP=clang build.c src/core/build.c src/cup/build.c src/cup/in_repo.c src/cup/c_toolchain/build.c src/cup/bootstrap.c%ARCH% -g -O0 -Isrc -o cup.exe -Wno-deprecated-declarations -Wno-microsoft-anon-tag -Xlinker /noimplib -Xlinker /incremental:no -Xlinker /pdb:build/cup.exe.pdb"

%CC_CLANG_HASH% || exit /B 1
build\hash_gen.exe -o src/core/hash.h src/core/hash_gen.c || exit /B 1
%CC_CLANG_CUP% || exit /B 1

call :gen_compile_commands ^
    "src/core/hash_gen.c|CC_CLANG_HASH" ^
    "build.c|CC_CLANG_CUP"
echo compile_commands.json generated successfully.
exit /B 0

:build_msvc
if not exist build\obj mkdir build\obj
if not exist build\obj\src mkdir build\obj\src
if not exist build\obj\src\core mkdir build\obj\src\core
if not exist build\obj\src\cup mkdir build\obj\src\cup
if not exist build\obj\src\cup\c_toolchain mkdir build\obj\src\cup\c_toolchain

set "CC_MSVC_HASH=cl /nologo src/core/hash_gen.c /I src /c /Fobuild/hash_gen.obj"
set "CC_MSVC_ROOT=cl /Fo:build/obj/build.c.obj /c /nologo build.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/build.c.obj.pdb"
set "CC_MSVC_CORE=cl /Fo:build/obj/src/core/build.c.obj /c /nologo src/core/build.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/src/core/build.c.obj.pdb"
set "CC_MSVC_CUP=cl /Fo:build/obj/src/cup/build.c.obj /c /nologo src/cup/build.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/src/cup/build.c.obj.pdb"
set "CC_MSVC_REPO=cl /Fo:build/obj/src/cup/in_repo.c.obj /c /nologo src/cup/in_repo.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/src/cup/in_repo.c.obj.pdb"
set "CC_MSVC_CT=cl /Fo:build/obj/src/cup/c_toolchain/build.c.obj /c /nologo src/cup/c_toolchain/build.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/src/cup/c_toolchain/build.c.obj.pdb"
set "CC_MSVC_BOOT=cl /Fo:build/obj/src/cup/bootstrap.c.obj /c /nologo src/cup/bootstrap.c /Od /std:clatest /Isrc /Zi /Fdbuild/obj/src/cup/bootstrap.c.obj.pdb"

%CC_MSVC_HASH% || exit /B 1
link /nologo build\hash_gen.obj /out:build\hash_gen.exe /incremental:no /pdb:build\hash_gen.exe.pdb || exit /B 1
build\hash_gen.exe -o src\core\hash.h src\core\hash_gen.c || exit /B 1
%CC_MSVC_ROOT% || exit /B 1
%CC_MSVC_CORE% || exit /B 1
%CC_MSVC_CUP% || exit /B 1
%CC_MSVC_REPO% || exit /B 1
%CC_MSVC_CT% || exit /B 1
%CC_MSVC_BOOT% || exit /B 1
link /nologo build/obj/build.c.obj build/obj/src/core/build.c.obj build/obj/src/cup/build.c.obj build/obj/src/cup/in_repo.c.obj build/obj/src/cup/c_toolchain/build.c.obj build/obj/src/cup/bootstrap.c.obj /debug /out:cup.exe /incremental:no /pdb:build\cup.exe.pdb || exit /B 1

call :gen_compile_commands ^
    "src/core/hash_gen.c|CC_MSVC_HASH" ^
    "build.c|CC_MSVC_ROOT" ^
    "src/core/build.c|CC_MSVC_CORE" ^
    "src/cup/build.c|CC_MSVC_CUP" ^
    "src/cup/in_repo.c|CC_MSVC_REPO" ^
    "src/cup/c_toolchain/build.c|CC_MSVC_CT" ^
    "src/cup/bootstrap.c|CC_MSVC_BOOT"
echo compile_commands.json generated successfully.
exit /B 0

REM ---- Subroutine: generate compile_commands.json ----
REM Usage: call :gen_compile_commands "file|VARNAME" "file|VARNAME" ...
:gen_compile_commands
set "FIRST="
(
echo [
for %%p in (%*) do for /f "tokens=1,2 delims=|" %%a in ("%%~p") do (
    if not defined FIRST (set "FIRST=1" & call :write_entry "%%a" "%%b" "") else call :write_entry "%%a" "%%b" ", "
)
echo ]
) > compile_commands.json
goto :eof

REM ---- Subroutine: write one JSON entry ----
REM %1 = file path, %2 = variable name with command, %3 = separator (comma+space or empty)
:write_entry
set "FILE=%~1"
set "CMD_VAR=%~2"
set "SEP=%~3"
call set "CMD=%%%CMD_VAR%%%"
echo  %SEP%{ "directory": "%CD:\=/%", "file": "%FILE%", "command": "%CMD%" }
goto :eof
