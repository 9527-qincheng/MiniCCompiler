@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ============================================================================
rem  MiniC Compiler - One-click build script (Windows)
rem ----------------------------------------------------------------------------
rem  Usage:
rem    build.bat                     build, output build\minic.exe
rem    build.bat clean               remove build directory
rem    build.bat rebuild             clean then build
rem    build.bat run tests\test.mc   build then run given source
rem    build.bat list                list source files to be compiled
rem ----------------------------------------------------------------------------
rem  Notes:
rem    - Auto-scans all .cpp under src\, no need to edit this script
rem    - On failure, prints hints (duplicate symbols / missing files)
rem    - Behavior matches GLOB_RECURSE in CMakeLists.txt
rem ============================================================================

set "ROOT=%~dp0"
set "SRCDIR=%ROOT%src"
set "BUILDDIR=%ROOT%build"
set "OUT=%BUILDDIR%\minic.exe"

rem ---------- dispatch ----------
rem  NOTE: capture args BEFORE any shift, and pass explicit "build" marker
rem        to the recursive call so it does not re-enter :run
if /i "%~1"=="clean"   goto :clean
if /i "%~1"=="rebuild" goto :rebuild
if /i "%~1"=="list"    goto :list
if /i "%~1"=="run"     goto :run
if /i "%~1"=="build"   goto :build
if "%~1"==""           goto :build
goto :usage

rem ---------- environment check ----------
:build
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Please install MinGW and add it to PATH.
    exit /b 1
)
if not exist "%SRCDIR%" (
    echo [ERROR] src directory not found: %SRCDIR%
    echo         Run this script from the project root ^(same level as src^).
    exit /b 1
)

rem ---------- collect sources ----------
set "SRC="
set "COUNT=0"
for /r "%SRCDIR%" %%f in (*.cpp) do (
    set "SRC=!SRC! "%%f""
    set /a COUNT+=1
)

if !COUNT!==0 (
    echo [ERROR] No .cpp file found under src
    exit /b 1
)

echo ============================================================
echo  MiniC Compiler - BUILD
echo ============================================================
echo   Source dir : %SRCDIR%
echo   File count : !COUNT!
echo   Output     : %OUT%
echo ------------------------------------------------------------
for /r "%SRCDIR%" %%f in (*.cpp) do echo   - %%f
echo ------------------------------------------------------------
echo.

if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"

rem ---------- compile ----------
echo [1/2] Compiling ^(-std=c++17 -Wall -Wextra^) ...
g++ -std=c++17 -Wall -Wextra -I"%SRCDIR%" -o "%OUT%" !SRC!
if errorlevel 1 goto :faillink

if not exist "%OUT%" goto :failnoexe

rem zero-size output means link aborted
for %%A in ("%OUT%") do set "SIZE=%%~zA"
if !SIZE! LEQ 0 (
    echo.
    echo [FAILED] Output exe is 0 bytes - link did not complete.
    goto :faillink
)

echo.
echo [2/2] Done
echo ============================================================
echo  BUILD SUCCESS
echo  Output: %OUT%  ^(!SIZE! bytes^)
echo ============================================================
echo.
echo Run examples:
echo   build.bat run tests\test.mc
echo   build\minic.exe tests\test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads
echo.
endlocal
exit /b 0

rem ============================================================================
rem  error handling
rem ============================================================================
:faillink
echo.
echo ============================================================
echo  BUILD FAILED
echo ============================================================
echo.
echo Common causes:
echo.
echo   [1] multiple definition of 'X'; first defined here
echo       ^-^> NAME COLLISION: two .cpp files define the same class/function
echo          Check : grep -rn "class X" src
echo          Case  : src/symbol/symbol.cpp and src/symbol/symbol_table.cpp
echo                  both defined class SymbolTable, breaking the whole link
echo.
echo   [2] undefined reference to 'X'
echo       ^-^> MISSING FILE: some .cpp was not compiled in
echo          Check : make sure the file is under src (auto-collected here)
echo.
echo For detailed errors:
echo   g++ wraps ld via collect2, which may only show "ld returned 1".
echo   To see the real message, compile each file then call ld directly:
echo     g++ -std=c++17 -Isrc -c src\lexer\token.cpp -o token.o
echo     ld -o build\minic.exe *.o
echo.
endlocal
exit /b 1

:failnoexe
echo.
echo [FAILED] Executable not generated: %OUT%
endlocal
exit /b 1

rem ============================================================================
rem  usage
rem ============================================================================
:usage
echo MiniC Compiler build script
echo.
echo Usage:
echo   build.bat                     build project
echo   build.bat clean               remove build directory
echo   build.bat rebuild             clean then build
echo   build.bat list                list source files
echo   build.bat run ^<source.mc^>    build then run
echo.
endlocal
exit /b 1

rem ============================================================================
rem  clean / rebuild / list / run
rem ============================================================================
:clean
if exist "%BUILDDIR%" (
    echo [INFO] Cleaning %BUILDDIR%
    rmdir /s /q "%BUILDDIR%"
    echo [DONE] Cleaned
) else (
    echo [INFO] build directory does not exist, nothing to clean
)
endlocal
exit /b 0

:rebuild
call "%~f0" clean
call "%~f0" build
endlocal
exit /b %errorlevel%

:list
set "COUNT=0"
for /r "%SRCDIR%" %%f in (*.cpp) do (
    set /a COUNT+=1
    echo   !COUNT!. %%f
)
echo.
echo   Total !COUNT! source file^(s^) will be compiled
endlocal
exit /b 0

:run
rem  Everything after "run" is the program argument list.
rem  %* still holds the full original line, so strip the leading "run ".
set "RUNARGS=%*"
set "RUNARGS=%RUNARGS:*run =%"
if "%RUNARGS%"=="" (
    echo [ERROR] Usage: build.bat run ^<source.mc^> [options]
    echo         e.g. : build.bat run tests\test.mc
    endlocal
    exit /b 1
)
echo.
echo [BUILD] building before run ...
call "%~f0" build
if errorlevel 1 (
    endlocal
    exit /b 1
)
echo.
echo [RUN] "%OUT%" %RUNARGS%
echo ------------------------------------------------------------
"%OUT%" %RUNARGS%
set "RC=%errorlevel%"
echo ------------------------------------------------------------
echo [EXIT CODE] %RC%
endlocal
exit /b %RC%
