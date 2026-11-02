@echo off
chcp 65001 >nul
rem ============================================================
rem  MiniC Compiler - one-click setup and launch
rem
rem  做的事：
rem    1. 检查 g++      （缺失则提示安装方法，装好后可重新检测）
rem    2. 检查 node/npm （同上）
rem    3. 检查 Electron （缺失则调用 npm install）
rem    4. 检查 minic.exe（缺失则调用 build.bat build）
rem    5. 启动 gui\启动GUI.bat
rem
rem  任何一步缺失都会先问你要不要处理；拒绝则提示「无法运行」并
rem  停在窗口里等你按键，不会自己一闪而过。
rem ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "GUI_DIR=%ROOT%\gui"
set "EXE=%ROOT%\build\minic.exe"
set "ELEC=%GUI_DIR%\node_modules\electron\dist\electron.exe"

echo ============================================================
echo   MiniC Compiler 一键配置并启动
echo ============================================================
echo   项目目录：%ROOT%
echo.

rem ================= 1/4 g++ =================
:check_gpp
echo [1/4] 检查 C++ 编译器 g++ ...
where g++ >nul 2>nul
if not errorlevel 1 goto gpp_ok
echo.
echo     [缺失] 未找到 g++，编译 minic.exe 需要它。
echo            安装方法：先装 MSYS2，再执行
echo              pacman -S mingw-w64-ucrt-x86_64-gcc
echo            下载地址：https://www.msys2.org/
echo            装好后确认 C:\msys64\ucrt64\bin 已加进 PATH。
echo.
choice /c YN /n /m "    已经装好了，重新检测? [Y=重新检测 / N=退出] "
if errorlevel 2 goto cannot_run
echo.
goto check_gpp

:gpp_ok
for /f "delims=" %%v in ('g++ --version 2^>nul') do (
    echo     OK  %%v
    goto gpp_done
)
:gpp_done
echo.

rem ================= 2/4 node =================
:check_node
echo [2/4] 检查 Node.js / npm ...
where node >nul 2>nul
if errorlevel 1 goto node_missing
where npm >nul 2>nul
if errorlevel 1 goto node_missing
for /f "delims=" %%v in ('node --version 2^>nul') do echo     OK  Node %%v
goto node_ok

:node_missing
echo.
echo     [缺失] 未找到 Node.js 或 npm，GUI 需要它。
echo            下载地址：https://nodejs.org/  （选 LTS 版本）
echo            安装时请勾选 "Add to PATH"。
echo.
choice /c YN /n /m "    已经装好了，重新检测? [Y=重新检测 / N=退出] "
if errorlevel 2 goto cannot_run
echo.
goto check_node

:node_ok
echo.

rem ================= 3/4 Electron =================
:check_elec
echo [3/4] 检查 GUI 依赖 Electron ...
if exist "%ELEC%" goto elec_ok
echo.
echo     [缺失] gui\node_modules 尚未安装（Electron 约 100 MB）。
echo            这一步需要联网，首次约 5-10 分钟。
echo            仓库已配好国内镜像 registry.npmmirror.com。
echo.
choice /c YN /n /m "    现在自动安装? [Y=安装 / N=退出] "
if errorlevel 2 goto cannot_run
echo.
echo     正在执行 npm install，请勿关闭窗口 ...
pushd "%GUI_DIR%"
call npm install
set "NPM_RC=!errorlevel!"
popd
if not "!NPM_RC!"=="0" (
    echo.
    echo     [失败] npm install 返回 %NPM_RC%
    echo            也可以手动执行：
    echo              cd /d "%GUI_DIR%"
    echo              npm install
    echo.
    choice /c YN /n /m "    重试一次? [Y=重试 / N=退出] "
    if errorlevel 2 goto cannot_run
    goto check_elec
)
if not exist "%ELEC%" (
    echo     [失败] 安装完成但找不到 electron.exe。
    echo.
    choice /c YN /n /m "    重试一次? [Y=重试 / N=退出] "
    if errorlevel 2 goto cannot_run
    goto check_elec
)

:elec_ok
echo     OK  Electron 已就绪
echo.

rem ================= 4/4 minic.exe =================
:check_exe
echo [4/4] 检查编译器 minic.exe ...
if exist "%EXE%" goto exe_ok
echo.
echo     [缺失] build\minic.exe 尚未编译（约需 20-60 秒）。
echo.
choice /c YN /n /m "    现在自动编译? [Y=编译 / N=退出] "
if errorlevel 2 goto cannot_run
echo.
echo     正在编译，请勿关闭窗口 ...
echo.
call "%ROOT%\build.bat" build
if errorlevel 1 (
    echo.
    echo     [失败] 编译失败，请查看上面的错误信息。
    echo.
    choice /c YN /n /m "    重试一次? [Y=重试 / N=退出] "
    if errorlevel 2 goto cannot_run
    goto check_exe
)
if not exist "%EXE%" (
    echo     [失败] 编译完成但找不到 minic.exe。
    echo.
    choice /c YN /n /m "    重试一次? [Y=重试 / N=退出] "
    if errorlevel 2 goto cannot_run
    goto check_exe
)

:exe_ok
for %%f in ("%EXE%") do echo     OK  minic.exe  %%~zf 字节
echo.

rem ================= 启动 =================
echo ============================================================
echo   环境已就绪，正在启动 GUI ...
echo   首次启动会把 Electron 运行时复制到本地目录，请稍候。
echo ============================================================
echo.
if defined MINIC_SETUP_NO_LAUNCH (
    echo   [测试模式] 已跳过启动。
    exit /b 0
)
call "%GUI_DIR%\启动GUI.bat"
exit /b 0

rem ================= 无法运行 =================
:cannot_run
echo.
echo ============================================================
echo   [无法运行] 缺少必要的运行环境，GUI 无法启动。
echo ------------------------------------------------------------
echo   请按上面的提示装好缺少的组件，再重新双击本脚本。
echo ============================================================
echo.
echo   按任意键关闭本窗口 ...
pause >nul
exit /b 1
