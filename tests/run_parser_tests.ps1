#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— Parser 回归测试
# 用法：
#   pwsh -File tests\run_parser_tests.ps1
#   pwsh -File tests\run_parser_tests.ps1 -Update
#
# 说明：
#   - 对 tests\parser\*.mc 逐个运行 `minic <case>.mc --dump-ast --dump-parse`
#   - 校验：退出码、AST 输出（<case>.ast.txt）、错误输出（stderr）
#   - 期望结果存放于 tests\parser\expected\<case>.ast.txt / <case>.err.txt
#   - 若存在 expected\<case>.parse.txt，则额外逐字比对 LL(1) 分析轨迹（§4.11）；
#     该 golden 由设计文档 §4.11 表格提取，不随 -Update 刷新
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
    # 挑战版文法：`int a, b;` 合法（doc §4.11 示例输入即为此），故由 err_ 用例转为正向用例；
    # 声明符列表的错误路径改由 err_decl_tail_no_name（`int a, ;`）覆盖
    @{ name = "multi_decl";           exit = 0 },
    @{ name = "err_decl_tail_no_name"; exit = 1 },
    # 扩展文法：一元运算与控制流（挑战版第一批，不含比较运算符）
    @{ name = "unary_ops";            exit = 0 },
    @{ name = "if_else";              exit = 0 },
    @{ name = "while_block";          exit = 0 },
    @{ name = "return_stmt";          exit = 0 },
    @{ name = "nested_if";            exit = 0 },
    @{ name = "err_if_missing_paren"; exit = 1 },
    @{ name = "err_block_unclosed";   exit = 1 },
    # 挑战版语法补充用例：比较/逻辑运算符、bool/char/print、数组、函数、嵌套结构
    @{ name = "compare_ops";          exit = 0 },
    @{ name = "logic_ops";            exit = 0 },
    @{ name = "bool_char";            exit = 0 },
    @{ name = "print_stmt";           exit = 0 },
    @{ name = "array_1d";             exit = 0 },
    @{ name = "func_def";             exit = 0 },
    @{ name = "nested_ctrl";          exit = 0 },
    # 收尾清单（2026-10-21）：全特性叠加用例——一维数组+下标赋值+函数定义(形参)+while+if-else+print+return
    @{ name = "full_challenge";       exit = 0 },
    # 挑战版新语法的非法输入
    @{ name = "err_print_no_paren";   exit = 1 },
    @{ name = "err_array_no_size";    exit = 1 },
    @{ name = "err_decl_after_stmt";  exit = 1 },
    @{ name = "err_2d_array";         exit = 1 },
    @{ name = "err_func_bad_param";   exit = 1 },
    # §4.11 验收：输入即文档 82 步 golden trace 的示例；
    # parse = $true 表示另外逐字比对 expected\ll1_411.parse.txt（该 golden 直接取自文档 §4.11 表格）
    @{ name = "ll1_411";              exit = 0; parse = $true }
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

# 临时目录建在仓库内（build\ 已被 .gitignore 排除），不依赖系统 %TEMP%。
# 受限环境（沙箱/精简权限）下 %TEMP% 可能不可写，症状为 41 个用例全 FAIL 并报
# `minic: error: 0:0: cannot write output file: ...ll1_411.parse.txt` —— 属假失败。
$scratch = Join-Path $repoRoot ("build\_parser_scratch_" + [System.Guid]::NewGuid().ToString("N"))
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
        -ArgumentList @("`"$workSrc`"", "--dump-ast", "--dump-parse") `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $exitCode = $proc.ExitCode
    # 受限环境（Windows 沙箱 / 精简权限）下 Start-Process -PassThru 取不到 .ExitCode，
    # 返回 $null，症状是全部用例都报 `exit code: expected 0, got `（got 后为空）。
    # 此时用直接调用重跑一次拿真实退出码（& 调用在当前进程内执行，$LASTEXITCODE 可靠）。
    if ($null -eq $exitCode) {
        & $Exe $workSrc --dump-ast --dump-parse 1>$outFile 2>$errFile | Out-Null
        $exitCode = $LASTEXITCODE
    }
    $astPath = [System.IO.Path]::ChangeExtension($workSrc, ".ast.txt")
    $parsePath = [System.IO.Path]::ChangeExtension($workSrc, ".parse.txt")
    return @{
        code  = $exitCode
        ast   = (Read-Text $astPath)
        parse = (Read-Text $parsePath)
        err   = (Read-Text $errFile)
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
    $parseGoldenPath = Join-Path $goldenDir ($c.name + ".parse.txt")

    if ($Update) {
        Write-Golden $astGoldenPath $r.ast
        Write-Golden $errGoldenPath $r.err
        # 注意：*.parse.txt golden 取自设计文档 §4.11，属"外部权威基线"，
        # 不用程序自身输出刷新（否则等于自我印证）；如需更新请手工从文档提取。
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
    # LL(1) 分析轨迹（仅对声明了 parse golden 的用例比对）
    if (Test-Path -LiteralPath $parseGoldenPath) {
        $expParse = Normalize (Read-Text $parseGoldenPath)
        $gotParse = Normalize $r.parse
        if ($expParse -ne $gotParse) {
            $problems += "parse trace mismatch (`"$parseGoldenPath`")"
        }
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
