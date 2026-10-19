@echo off
rem ============================================================================
rem  MiniC GUI - build script (Windows / MinGW g++)
rem
rem  Usage:
rem    build_gui.bat            build gui\minic_gui.exe
rem    build_gui.bat clean      remove gui\minic_gui.exe
rem    build_gui.bat run        build then launch the GUI
rem    build_gui.bat all        build minic.exe (repo root) then the GUI
rem ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "GUIDIR=%~dp0"
set "ROOT=%~dp0.."
set "OUT=%GUIDIR%minic_gui.exe"
set "SRC=%GUIDIR%minic_gui.cpp"

if /i "%~1"=="clean" goto :clean
if /i "%~1"=="run"   goto :run
if /i "%~1"=="all"   goto :all
goto :build

rem --------------------------------------------------------------------------
:check
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Please install MinGW and add it to PATH.
    exit /b 1
)
if not exist "%SRC%" (
    echo [ERROR] source not found: %SRC%
    exit /b 1
)
exit /b 0

rem --------------------------------------------------------------------------
:build
call :check
if errorlevel 1 exit /b 1

echo [GUI] compiling %SRC%
rem NOTE: do NOT add -municode here. With -municode the MinGW runtime looks for
rem wWinMain, but this program uses the classic WinMain entry point; linking
rem then fails with a bare "collect2.exe: error: ld returned 1 exit status".
rem All Windows API calls are explicitly the *W variants, so ANSI WinMain is fine.
g++ -std=c++17 -O2 -mwindows -Wall -Wextra ^
    -o "%OUT%" "%SRC%" -lcomctl32 -lcomdlg32 -lgdi32 -luser32

if errorlevel 1 (
    echo.
    echo [FAILED] build failed.
    endlocal & exit /b 1
)
if not exist "%OUT%" (
    echo [FAILED] output exe was not produced.
    endlocal & exit /b 1
)

for %%A in ("%OUT%") do set "SIZE=%%~zA"
echo.
echo [OK] output: %OUT%  (!SIZE! bytes)
echo.
if not exist "%ROOT%\build\minic.exe" (
    echo [WARN] %ROOT%\build\minic.exe not found.
    echo        The GUI needs it. Run build.bat in the repo root first,
    echo        or use: build_gui.bat all
)
endlocal & exit /b 0

rem --------------------------------------------------------------------------
:clean
if exist "%OUT%" (
    del /q "%OUT%"
    echo [OK] removed %OUT%
) else (
    echo [INFO] nothing to clean.
)
endlocal & exit /b 0

rem --------------------------------------------------------------------------
:run
call :build
if errorlevel 1 exit /b 1
if not exist "%OUT%" (
    echo [ERROR] GUI exe missing.
    endlocal & exit /b 1
)
start "" "%OUT%"
endlocal & exit /b 0

rem --------------------------------------------------------------------------
:all
if not exist "%ROOT%\build.bat" (
    echo [ERROR] build.bat not found in %ROOT%
    endlocal & exit /b 1
)
echo [1/2] building the compiler ...
pushd "%ROOT%"
call build.bat
set "RC=!errorlevel!"
popd
if not "!RC!"=="0" (
    echo [FAILED] compiler build failed.
    endlocal & exit /b 1
)
echo.
echo [2/2] building the GUI ...
call :build
endlocal & exit /b 0
