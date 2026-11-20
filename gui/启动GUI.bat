@echo off
chcp 65001 >nul
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
    echo.
    echo   最简单的办法：关掉本窗口，到项目根目录双击
    echo     "一键配置并启动.bat"
    echo   它会自动检查并装好所有缺失的组件，然后启动 GUI。
    echo.
    echo   或者手动执行：
    echo     cd /d "%APP_DIR%"  ^&^&  npm install
    pause
    exit /b 1
)

rem -- build React renderer bundle before launching --
where npm >nul 2>nul
if errorlevel 1 (
    echo [ERROR] npm not found. Install Node.js first.
    pause
    exit /b 1
)
echo [1/3] Building UI bundle ...
pushd "%APP_DIR%"
call npm run build
set "BUILD_RC=%errorlevel%"
popd
if not "%BUILD_RC%"=="0" (
    echo [ERROR] UI build failed.
    pause
    exit /b 1
)

if not exist "%RUNTIME%\electron.exe" (
    echo [2/3] Deploying runtime to %RUNTIME% ...
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

echo [3/3] Starting MiniC Studio ...
start "" "%RUNTIME%\electron.exe" "%APP_DIR%" --no-sandbox --disable-gpu
endlocal
