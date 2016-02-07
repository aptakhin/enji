@echo off

set enji_dir=%~dp0

echo if not exist "%enji_dir%enji-deps-0.0.0.zip"
if not exist "%enji_dir%enji-deps-0.0.0.zip" (
    powershell -Command "Invoke-WebRequest https://github.com/aptakhin/enji/releases/download/0.0.0/enji-deps-0.0.0.zip -OutFile enji-deps-0.0.0.zip"
)


rem
rem Copypaste of settings from deps\packages\libuv-1.8.0\vcbuild.bat
rem

if /i "%1"=="help" goto help
if /i "%1"=="--help" goto help
if /i "%1"=="-help" goto help
if /i "%1"=="/help" goto help
if /i "%1"=="?" goto help
if /i "%1"=="-?" goto help
if /i "%1"=="--?" goto help
if /i "%1"=="/?" goto help

@rem Process arguments.
set config=
set target=Build
set noprojgen=
set nobuild=
set run=
set target_arch=ia32
set vs_toolset=x86
set platform=WIN32
set library=static_library

set that_args=%1 %2 %3 %4 %5 %6 %7 %8 %9

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"        set config=Debug&goto arg-ok
if /i "%1"=="release"      set config=Release&goto arg-ok
if /i "%1"=="test"         set run=run-tests.exe&goto arg-ok
if /i "%1"=="bench"        set run=run-benchmarks.exe&goto arg-ok
if /i "%1"=="clean"        set target=Clean&goto arg-ok
if /i "%1"=="noprojgen"    set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"      set nobuild=1&goto arg-ok
if /i "%1"=="x86"          set target_arch=ia32&set platform=WIN32&set vs_toolset=x86&goto arg-ok
if /i "%1"=="ia32"         set target_arch=ia32&set platform=WIN32&set vs_toolset=x86&goto arg-ok
if /i "%1"=="x64"          set target_arch=x64&set platform=x64&set vs_toolset=x64&goto arg-ok
if /i "%1"=="shared"       set library=shared_library&goto arg-ok
if /i "%1"=="static"       set library=static_library&goto arg-ok
:arg-ok
shift
goto next-arg
:args-done

rem
rem End of libuv copypaste
rem

set libuv_dir=deps\packages\libuv-1.8.0
call "%libuv_dir%\vcbuild.bat" %that_args%
cd %enji_dir%

copy "%libuv_dir%\Debug\lib\libuv.lib"   "%enji_dir%\deps\lib\debug\libuv.lib"
copy "%libuv_dir%\Release\lib\libuv.lib" "%enji_dir%\deps\lib\release\libuv.lib"
