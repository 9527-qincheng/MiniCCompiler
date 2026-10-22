#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— DumpSymTab 回归测试
# 用法：
#   pwsh -File tests\run_symtab_tests.ps1
#   pwsh -File tests\run_symtab_tests.ps1 -Update
#
# 说明：
#   - 编译 tests\test.mc，运行 `minic <test.mc> --dump-symtab`
#   - 校验：生成的 .sym.txt 与期望 tests\test.sym.txt 逐字一致
#   - .sym.txt 是四类必交产物之一，这里是它的回归网：一旦有人改 DumpSymTab 的
#     输出格式而不同步刷新 test.sym.txt，本测试会失败，防止格式悄悄漂移
#   - 同时要求退出码为 0（语义分析正常通过才会产出登记后的符号表）
#   - `-Update` 用当前程序输出刷新期望结果；**仅在确认格式变更是有意为之、
#     且已人工核对刷新后的符号表输出**才使用，否则等于自我印证
#   - 写入期望结果统一用 UTF-8 无 BOM（同 run_lexer_tests.ps1 的 Write-Golden）
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

$caseName = "test"
$srcPath  = Join-Path $scriptDir ($caseName + ".mc")       # tests\test.mc
$golden   = Join-Path $scriptDir ($caseName + ".sym.txt")  # tests\test.sym.txt

if (-not (Test-Path -LiteralPath $srcPath)) {
    Write-Host "FAIL: missing input $srcPath" -ForegroundColor Red
    exit 2
}

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

# 临时目录：建在仓库内（build\_symtab_scratch），避免受限环境下 %TEMP% 不可写
# 导致"假失败"（症状：cannot write output file: ...sym.txt）。
$scratch = Join-Path $repoRoot "build\_symtab_scratch"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

$workSrc  = Join-Path $scratch ($caseName + ".mc")
Copy-Item -LiteralPath $srcPath -Destination $workSrc -Force
$outFile = Join-Path $scratch ($caseName + ".stdout.txt")
$errFile = Join-Path $scratch ($caseName + ".stderr.txt")
$proc = Start-Process -FilePath $Exe `
    -ArgumentList @("`"$workSrc`"", "--dump-symtab") `
    -Wait -NoNewWindow -PassThru `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile
# 产物与源文件同目录：build\_symtab_scratch\test.sym.txt
$symPath = [System.IO.Path]::ChangeExtension($workSrc, ".sym.txt")
$got = Read-Text $symPath
$err = Read-Text $errFile

Remove-Item -LiteralPath $scratch -Recurse -Force

if ($Update) {
    if ($null -eq $got) {
        Write-Host "FAIL: no .sym.txt produced (compile failed?)" -ForegroundColor Red
        Write-Host "      stderr:" -ForegroundColor Red
        Write-Host $err -ForegroundColor Red
        exit 2
    }
    Write-Golden $golden $got
    Write-Host ("Updated {0}.sym.txt golden. 请人工核对刷新后的符号表输出。" -f $caseName) -ForegroundColor Yellow
    exit 0
}

$exp  = Normalize (Read-Text $golden)
$gotN = Normalize $got

$problems = @()
if ($exp -ne $gotN) {
    $problems += "symtab dump mismatch (`"$golden`")"
}
if ($proc.ExitCode -ne 0) {
    $problems += "expected exit code 0, got $($proc.ExitCode)"
}

if ($problems.Count -eq 0) {
    Write-Host "PASS  symtab dump regression ($caseName)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL  symtab dump regression ($caseName)" -ForegroundColor Red
    foreach ($p in $problems) { Write-Host ("      - " + $p) -ForegroundColor Red }
    if ($exp -ne $gotN) {
        Write-Host "      --- expected test.sym.txt ---" -ForegroundColor DarkGray
        Write-Host $exp -ForegroundColor DarkGray
        Write-Host "      --- actual --dump-symtab ---" -ForegroundColor DarkGray
        Write-Host $gotN -ForegroundColor DarkGray
    }
    if ($proc.ExitCode -ne 0) {
        Write-Host "      --- actual stderr ---" -ForegroundColor DarkGray
        Write-Host $err -ForegroundColor DarkGray
    }
    exit 1
}
