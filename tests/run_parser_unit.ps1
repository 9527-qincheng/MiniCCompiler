#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— Parser 单元测试运行器
# 编译 tests\parser_unit_test.cpp（仅依赖 lexer + parser）并在若干输入上运行。
# 用法：
#   pwsh -File tests\run_parser_unit.ps1
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
# （受限环境下 %TEMP% 不可写会导致编译/运行假失败）。
$scratch = Join-Path $repoRoot ("build\_parser_unit_scratch_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
$exe = Join-Path $scratch "parser_unit_test.exe"

$sources = @(
    (Join-Path $scriptDir "parser_unit_test.cpp"),
    (Join-Path $srcDir "lexer\token.cpp"),
    (Join-Path $srcDir "lexer\lexer.cpp"),
    (Join-Path $srcDir "parser\ast.cpp"),
    (Join-Path $srcDir "parser\grammar.cpp"),
    (Join-Path $srcDir "parser\parser.cpp")
)

Write-Host "Compiling parser_unit_test ..."
& $cxxPath -std=c++17 -I $srcDir -o $exe @sources
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: compile error" -ForegroundColor Red
    Remove-Item -LiteralPath $scratch -Recurse -Force
    exit 2
}

$inputs = @(
    "prec_add_mul.mc",
    "assoc_sub.mc",
    "assoc_div.mc",
    "mix_type.mc",
    "deep_paren.mc",
    "unary_ops.mc",
    "if_else.mc",
    "while_block.mc",
    "return_stmt.mc",
    "nested_if.mc",
    "compare_ops.mc",
    "logic_ops.mc",
    "bool_char.mc",
    "print_stmt.mc",
    "array_1d.mc",
    "func_def.mc",
    "nested_ctrl.mc"
)

$failures = 0
foreach ($name in $inputs) {
    $path = Join-Path $scriptDir ("parser\" + $name)
    Write-Host ""
    Write-Host ("--- {0} ---" -f $name) -ForegroundColor Cyan
    & $exe $path
    if ($LASTEXITCODE -ne 0) { $failures++ }
}

Remove-Item -LiteralPath $scratch -Recurse -Force

Write-Host ""
if ($failures -eq 0) {
    Write-Host ("All {0} parser unit input(s) passed." -f $inputs.Count) -ForegroundColor Green
    exit 0
} else {
    Write-Host ("{0}/{1} parser unit input(s) FAILED." -f $failures, $inputs.Count) -ForegroundColor Red
    exit 1
}
