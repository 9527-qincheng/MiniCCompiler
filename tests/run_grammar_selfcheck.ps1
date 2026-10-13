# ============================================================================
# MiniC 编译器 —— 文法表自检运行器
# 编译 tests\grammar_selfcheck.cpp（仅依赖 src\parser\grammar.*）并运行。
# 用法：
#   powershell -ExecutionPolicy Bypass -File tests\run_grammar_selfcheck.ps1
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

$scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("minic_grammar_selfcheck_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
$exe = Join-Path $scratch "grammar_selfcheck.exe"

$sources = @(
    (Join-Path $scriptDir "grammar_selfcheck.cpp"),
    (Join-Path $srcDir "parser\grammar.cpp")
)

Write-Host "Compiling grammar_selfcheck ..."
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
    Write-Host "Grammar self-check passed." -ForegroundColor Green
} else {
    Write-Host "Grammar self-check FAILED." -ForegroundColor Red
}
exit $code
