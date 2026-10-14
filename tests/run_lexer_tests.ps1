#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— Lexer 回归测试
# 用法：
#   pwsh -File tests\run_lexer_tests.ps1
#   pwsh -File tests\run_lexer_tests.ps1 -Update
#
# 说明：
#   - 对每个用例运行 `minic <case>.mc --dump-tokens`
#   - 校验：<case>.token.txt 词法流与期望逐字一致
#   - 期望结果存放于 tests\<case>.token.txt（与 *.mc 同目录，沿用词法基线既有位置）
#   - 为何不校验退出码：
#       lex_char / lex_challenge / lex_op_edge 是**刻意的词法片段**（如 `a == b;`、
#       `char a = 'a';` 单独成行），语法阶段必然报错并返回 1。该退出码来自 parser
#       而非 lexer，拿它判定词法正确性会产生误导（词法全对也会"失败"）。
#       故词法正确性**只由 token 流判定**；退出码仅打印供参考，不参与断言。
#   - 对声明 lexError = $true 的用例，额外要求 stderr 至少含一条 `minic: error:`，
#     防止词法把非法输入静默吞掉、却输出一个"干净但不完整"的 token 流
#     （这正是 bad_char/bad_op 曾经的缺陷形态）。
#   - `-Update` 用当前程序输出刷新期望结果；**仅在确认行为变更是有意为之、
#     且已人工核对刷新后的 token 流**才使用，否则等于自我印证。
#   - 写入期望结果统一用 UTF-8 无 BOM（同 run_parser_tests.ps1 的 Write-Golden）。
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
    Write-Host "      build first: build.bat build"
    exit 2
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path

# name             : tests\<name>.mc 输入
# lexError = $true : 期望 stderr 至少有一条 minic: error:（词法应报错，不得静默通过）
$cases = @(
    @{ name = "test";          },
    @{ name = "lex_char";      },
    @{ name = "lex_challenge"; },
    @{ name = "lex_op_edge";   },
    @{ name = "bad_char";      lexError = $true },
    @{ name = "bad_op";        lexError = $true }
)

function Read-Text([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    # ReadAllText 会自动剥离 UTF-8 BOM，故基线带不带 BOM 都能正确比对
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

# 临时目录：优先建在仓库内（build\_lexer_scratch），避免受限环境下 %TEMP% 不可写
# 导致"假失败"（症状：cannot write output file: ...token.txt）。
$scratch = Join-Path $repoRoot "build\_lexer_scratch"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

$failures = @()
$updated  = @()
$ran      = 0

function Invoke-Case([string]$srcPath, [string]$tag) {
    $workSrc = Join-Path $scratch ($tag + ".mc")
    Copy-Item -LiteralPath $srcPath -Destination $workSrc -Force
    $outFile = Join-Path $scratch ($tag + ".stdout.txt")
    $errFile = Join-Path $scratch ($tag + ".stderr.txt")
    $proc = Start-Process -FilePath $Exe `
        -ArgumentList @("`"$workSrc`"", "--dump-tokens") `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $tokPath = [System.IO.Path]::ChangeExtension($workSrc, ".token.txt")
    return @{
        code = $proc.ExitCode
        tok  = (Read-Text $tokPath)
        err  = (Read-Text $errFile)
    }
}

foreach ($c in $cases) {
    $ran++
    $srcPath    = Join-Path $scriptDir ($c.name + ".mc")
    $goldenPath = Join-Path $scriptDir ($c.name + ".token.txt")

    if (-not (Test-Path -LiteralPath $srcPath)) {
        $failures += "$($c.name): missing input $srcPath"
        continue
    }

    $r = Invoke-Case $srcPath $c.name

    if ($Update) {
        Write-Golden $goldenPath $r.tok
        $updated += $c.name
        continue
    }

    $problems = @()

    $expTok = Normalize (Read-Text $goldenPath)
    $gotTok = Normalize $r.tok
    if ($expTok -ne $gotTok) {
        $problems += "token stream mismatch (`"$goldenPath`")"
    }

    if ($c.lexError) {
        $hasError = ($r.err -match "minic: error:")
        if (-not $hasError) {
            $problems += "expected at least one 'minic: error:' on stderr, but lexer reported none"
        }
    }

    if ($problems.Count -eq 0) {
        Write-Host ("PASS  {0}" -f $c.name) -ForegroundColor Green
    } else {
        Write-Host ("FAIL  {0}" -f $c.name) -ForegroundColor Red
        foreach ($p in $problems) { Write-Host ("      - " + $p) -ForegroundColor Red }
        if ($expTok -ne $gotTok) {
            Write-Host "      --- expected tokens ---" -ForegroundColor DarkGray
            Write-Host $expTok -ForegroundColor DarkGray
            Write-Host "      --- actual tokens ---"   -ForegroundColor DarkGray
            Write-Host $gotTok -ForegroundColor DarkGray
        }
        if ($c.lexError -and -not ($r.err -match "minic: error:")) {
            Write-Host "      --- actual stderr ---" -ForegroundColor DarkGray
            Write-Host $r.err -ForegroundColor DarkGray
        }
        $failures += $c.name
    }
}

Remove-Item -LiteralPath $scratch -Recurse -Force

Write-Host ""
if ($Update) {
    Write-Host ("Updated {0} lexer golden case(s) under tests\" -f $updated.Count) -ForegroundColor Yellow
    Write-Host "  注意：请人工核对刷新后的 token 流，勿把程序缺陷固化为基线。" -ForegroundColor Yellow
    exit 0
}

if ($failures.Count -eq 0) {
    Write-Host ("All {0} lexer regression case(s) passed." -f $ran) -ForegroundColor Green
    exit 0
} else {
    Write-Host ("{0}/{1} lexer regression case(s) FAILED: {2}" -f $failures.Count, $ran, ($failures -join ", ")) -ForegroundColor Red
    exit 1
}
