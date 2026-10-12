# ============================================================================
# MiniC 编译器 —— Parser 回归测试
# 用法：
#   powershell -ExecutionPolicy Bypass -File tests\run_parser_tests.ps1
#   powershell -ExecutionPolicy Bypass -File tests\run_parser_tests.ps1 -Update
#
# 说明：
#   - 对 tests\parser\*.mc 逐个运行 `minic <case>.mc --dump-ast`
#   - 校验三项：退出码、AST 输出（<case>.ast.txt）、错误输出（stderr）
#   - 期望结果存放于 tests\parser\expected\<case>.ast.txt / <case>.err.txt
#   - `-Update` 用当前程序输出刷新期望结果（仅在确认行为变更是有意为之才使用）
#   - 额外固定校验：tests\test.mc 的 AST 必须与 tests\test.ast.txt 逐字一致
# ============================================================================

param(
    [string]$Exe = "",
    [switch]$Update
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir

if (-not $Exe) {
    $Exe = Join-Path $repoRoot "build\minic.exe"
}
if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "FAIL: compiler not found: $Exe" -ForegroundColor Red
    Write-Host "      build first: cmake --build build"
    exit 2
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path

$casesDir  = Join-Path $scriptDir "parser"
$goldenDir = Join-Path $casesDir "expected"
if (-not (Test-Path -LiteralPath $goldenDir)) {
    New-Item -ItemType Directory -Force -Path $goldenDir | Out-Null
}

# name -> 期望退出码（AST/stderr 期望值从 expected\ 读取）
$cases = @(
    @{ name = "empty";                exit = 0 },
    @{ name = "decl_only";            exit = 0 },
    @{ name = "prec_add_mul";         exit = 0 },
    @{ name = "assoc_sub";            exit = 0 },
    @{ name = "assoc_div";            exit = 0 },
    @{ name = "mix_type";             exit = 0 },
    @{ name = "deep_paren";           exit = 0 },
    @{ name = "paren_drop";           exit = 0 },
    @{ name = "err_missing_semi_decl"; exit = 1 },
    @{ name = "err_missing_semi_stmt"; exit = 1 },
    @{ name = "err_bad_lhs";          exit = 1 },
    @{ name = "err_missing_operand";  exit = 1 },
    @{ name = "err_unbalanced_paren"; exit = 1 },
    @{ name = "err_extra_paren";      exit = 1 },
    @{ name = "err_stray_rbrace";     exit = 1 },
    @{ name = "err_recovery";         exit = 1 },
    @{ name = "err_decl_no_name";     exit = 1 },
    @{ name = "err_decl_num_name";    exit = 1 },
    @{ name = "err_multi_decl";       exit = 1 },
    # 扩展文法：一元运算与控制流（挑战版第一批，不含比较运算符）
    @{ name = "unary_ops";            exit = 0 },
    @{ name = "if_else";              exit = 0 },
    @{ name = "while_block";          exit = 0 },
    @{ name = "return_stmt";          exit = 0 },
    @{ name = "nested_if";            exit = 0 },
    @{ name = "err_if_missing_paren"; exit = 1 },
    @{ name = "err_block_unclosed";   exit = 1 }
)

function Read-Text([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    return [System.IO.File]::ReadAllText($path)
}

function Normalize([string]$text) {
    if ($null -eq $text) { return $null }
    $t = $text -replace "`r`n", "`n"
    $t = $t -replace "`r", "`n"
    return $t.TrimEnd("`n")
}

function Write-Golden([string]$path, [string]$text) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, (Normalize $text) + "`n", $utf8)
}

$scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("minic_parser_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

$failures = @()
$updated  = @()
$ran      = 0

function Invoke-Case([string]$srcPath, [string]$tag, [int]$expectedExit) {
    $workSrc = Join-Path $scratch ($tag + ".mc")
    Copy-Item -LiteralPath $srcPath -Destination $workSrc -Force
    $outFile = Join-Path $scratch ($tag + ".stdout.txt")
    $errFile = Join-Path $scratch ($tag + ".stderr.txt")
    $proc = Start-Process -FilePath $Exe `
        -ArgumentList @("`"$workSrc`"", "--dump-ast") `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $astPath = [System.IO.Path]::ChangeExtension($workSrc, ".ast.txt")
    return @{
        code = $proc.ExitCode
        ast  = (Read-Text $astPath)
        err  = (Read-Text $errFile)
    }
}

foreach ($c in $cases) {
    $ran++
    $srcPath = Join-Path $casesDir ($c.name + ".mc")
    if (-not (Test-Path -LiteralPath $srcPath)) {
        $failures += "$($c.name): missing input $srcPath"
        continue
    }
    $r = Invoke-Case $srcPath $c.name $c.exit

    $astGoldenPath = Join-Path $goldenDir ($c.name + ".ast.txt")
    $errGoldenPath = Join-Path $goldenDir ($c.name + ".err.txt")

    if ($Update) {
        Write-Golden $astGoldenPath $r.ast
        Write-Golden $errGoldenPath $r.err
        $updated += $c.name
        continue
    }

    $problems = @()
    if ($r.code -ne $c.exit) {
        $problems += "exit code: expected $($c.exit), got $($r.code)"
    }
    $expAst = Normalize (Read-Text $astGoldenPath)
    $gotAst = Normalize $r.ast
    if ($expAst -ne $gotAst) {
        $problems += "AST mismatch (`"$astGoldenPath`")"
    }
    $expErr = Normalize (Read-Text $errGoldenPath)
    $gotErr = Normalize $r.err
    if ($expErr -ne $gotErr) {
        $problems += "stderr mismatch (`"$errGoldenPath`")"
    }

    if ($problems.Count -eq 0) {
        Write-Host ("PASS  {0}" -f $c.name) -ForegroundColor Green
    } else {
        Write-Host ("FAIL  {0}" -f $c.name) -ForegroundColor Red
        foreach ($p in $problems) { Write-Host ("      - " + $p) -ForegroundColor Red }
        if ($expAst -ne $gotAst) {
            Write-Host "      --- expected AST ---" -ForegroundColor DarkGray
            Write-Host $expAst -ForegroundColor DarkGray
            Write-Host "      --- actual AST ---"   -ForegroundColor DarkGray
            Write-Host $gotAst -ForegroundColor DarkGray
        }
        if ($expErr -ne $gotErr) {
            Write-Host "      --- expected stderr ---" -ForegroundColor DarkGray
            Write-Host $expErr -ForegroundColor DarkGray
            Write-Host "      --- actual stderr ---"   -ForegroundColor DarkGray
            Write-Host $gotErr -ForegroundColor DarkGray
        }
        $failures += $c.name
    }
}

# ---- 固定校验：既有黄金输出 tests\test.ast.txt 不得变化 ----
if (-not $Update) {
    $ran++
    $legacySrc = Join-Path $repoRoot "tests\test.mc"
    $legacyGolden = Join-Path $repoRoot "tests\test.ast.txt"
    $r = Invoke-Case $legacySrc "legacy_test_mc" 0
    $expAst = Normalize (Read-Text $legacyGolden)
    $gotAst = Normalize $r.ast
    if ($r.code -eq 0 -and $expAst -eq $gotAst) {
        Write-Host "PASS  legacy_test_mc (== tests/test.ast.txt)" -ForegroundColor Green
    } else {
        Write-Host "FAIL  legacy_test_mc (== tests/test.ast.txt)" -ForegroundColor Red
        if ($r.code -ne 0) { Write-Host "      - exit code: expected 0, got $($r.code)" -ForegroundColor Red }
        if ($expAst -ne $gotAst) {
            Write-Host "      --- expected ---" -ForegroundColor DarkGray
            Write-Host $expAst -ForegroundColor DarkGray
            Write-Host "      --- actual ---"   -ForegroundColor DarkGray
            Write-Host $gotAst -ForegroundColor DarkGray
        }
        $failures += "legacy_test_mc"
    }
}

Remove-Item -LiteralPath $scratch -Recurse -Force

Write-Host ""
if ($Update) {
    Write-Host ("Updated {0} golden case(s) under {1}" -f $updated.Count, $goldenDir) -ForegroundColor Yellow
    exit 0
}

if ($failures.Count -eq 0) {
    Write-Host ("All {0} parser regression case(s) passed." -f $ran) -ForegroundColor Green
    exit 0
} else {
    Write-Host ("{0}/{1} parser regression case(s) FAILED: {2}" -f $failures.Count, $ran, ($failures -join ", ")) -ForegroundColor Red
    exit 1
}
