#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— 符号表单元测试运行器
# 编译 tests\symbol_test.cpp（依赖 symbol + lexer/token + sema/ir）并运行。
# 覆盖：作用域链（内层遮蔽外层）、扁平下标稳定、数组登记、全局作用域不可弹出。
# 用法：
#   pwsh -File tests\run_symbol_tests.ps1
# 需要 g++ 在 PATH 中（可用 -Cxx 指定编译器路径）。
# ============================================================================

param(
    [string]$Cxx = "g++"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$srcDir    = Join-Path $repoRoot "src"

$cxxPath = (Get-Command $Cxx -ErrorAction SilentlyContinue).Source
if (-not $cxxPath) {
    Write-Host "FAIL: C++ compiler not found: $Cxx (pass -Cxx <path>)" -ForegroundColor Red
    exit 2
}

# 临时目录建在仓库内（build\ 已被 .gitignore 排除），不依赖系统 %TEMP%
$scratch = Join-Path $repoRoot ("build\_symbol_scratch_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
$exe = Join-Path $scratch "symbol_test.exe"

$sources = @(
    (Join-Path $scriptDir "symbol_test.cpp"),
    (Join-Path $srcDir "symbol\symbol.cpp"),
    (Join-Path $srcDir "lexer\token.cpp"),
    (Join-Path $srcDir "sema\ir.cpp")
)

Write-Host "Compiling symbol_test ..."
& $cxxPath -std=c++17 -Wall -Wextra -I $srcDir -o $exe @sources
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: compile error" -ForegroundColor Red
    Remove-Item -LiteralPath $scratch -Recurse -Force
    exit 2
}

& $exe
$code = $LASTEXITCODE

Remove-Item -LiteralPath $scratch -Recurse -Force

Write-Host ""
if ($code -eq 0) {
    Write-Host "Symbol scope-chain test passed." -ForegroundColor Green
} else {
    Write-Host "Symbol scope-chain test FAILED." -ForegroundColor Red
}
exit $code
