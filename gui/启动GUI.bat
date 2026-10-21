@echo off
rem ============================================================
rem  MiniC Compiler GUI launcher
rem ------------------------------------------------------------
rem  Why this script exists:
rem    Electron / minic.exe are native binaries. On this machine,
rem    running them from a path containing non-ASCII characters
rem    (e.g. E:\<Chinese>\MiniCCompiler) makes their file I/O fail
rem    or crash. Workaround: deploy the Electron runtime to an
rem    ASCII-only directory once, then launch it against the app
rem    folder (app folder may stay wherever it is).
rem ============================================================
setlocal
set "APP_DIR=%~dp0"
if "%APP_DIR:~-1%"=="\" set "APP_DIR=%APP_DIR:~0,-1%"
set "RUNTIME=%LOCALAPPDATA%\minic-gui\runtime"

rem -- clear inherited env that would force node-mode --
set "ELECTRON_RUN_AS_NODE="
set "NODE_OPTIONS="

if not exist "%APP_DIR%\node_modules\electron\dist\electron.exe" (
    echo [ERROR] Electron runtime not found.
    echo         Run:  cd /d "%APP_DIR%"  ^&^&  npm install
    pause
    exit /b 1
)

if not exist "%RUNTIME%\electron.exe" (
    echo [1/2] Deploying runtime to %RUNTIME% ...
    robocopy "%APP_DIR%\node_modules\electron\dist" "%RUNTIME%" /E /NFL /NDL /NJH /NJS /NP >nul
    if errorlevel 8 (
        echo [ERROR] Failed to deploy runtime.
        pause
        exit /b 1
    )
)

if not exist "%APP_DIR%\..\build\minic.exe" (
    echo [WARN] minic.exe not found - run build.bat first.
)

echo [2/2] Starting MiniC Compiler GUI ...
start "" "%RUNTIME%\electron.exe" "%APP_DIR%" --no-sandbox --disable-gpu
endlocal
