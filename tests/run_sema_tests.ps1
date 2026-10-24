#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— 语义分析（sema）诊断回归测试
# 用法：
#   pwsh -File tests\run_sema_tests.ps1
#   pwsh -File tests\run_sema_tests.ps1 -Case err_duplicate
#   pwsh -File tests\run_sema_tests.ps1 -Update
#
# 说明：
#   - 遍历 tests\sema\*.mc，逐个编译，比对退出码与 stderr 里的诊断行
#   - 用例文件第一行用 `/* expect: err | ok | warn <条数> */` 声明期望，
#     缺省为 ok。这样新加用例不必改 runner，只需在文件头写一行
#   - 比对的是**诊断行的完整文本**（`minic: error: 行:列: 消息`），
#     包括位置。只比"有几个错"会漏掉位置漂移；只比"有没有错"会漏掉措辞与
#     错误码变化。语义阶段是四段前端里唯一没有独立回归的，这层网就是为它补的
#   - 尾部汇总行（`N error(s) found, compilation failed`）不参与比对，
#     它由错误条数派生，逐条消息对上了它自然就对上
#   - err_ 前缀的用例必须退出码 1 且至少一条 error；ok_ 前缀必须退出码 0
#     且没有 error（warning 允许，warn 用例会额外核对条数）
#   - `-Update` 按当前程序输出重写每条用例的期望诊断块；仅在确认变更是
#     有意为之、且已人工核对后才使用，否则等于自我印证
# ============================================================================

param(
    [string]$Exe = "",
    [string]$Case = "",
    [switch]$Update
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$caseDir   = Join-Path $scriptDir "sema"

if (-not $Exe) {
    $Exe = Join-Path $repoRoot "build\minic.exe"
}
if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "FAIL: compiler not found: $Exe" -ForegroundColor Red
    Write-Host "      build first: build.bat build"
    exit 2
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path

if (-not (Test-Path -LiteralPath $caseDir)) {
    Write-Host "FAIL: missing case directory $caseDir" -ForegroundColor Red
    exit 2
}

function Read-Text([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    return [System.IO.File]::ReadAllText($path)
}

function NormalizeNewlines([string]$text) {
    if ($null -eq $text) { return "" }
    return (($text -replace "`r`n", "`n") -replace "`r", "`n")
}

function Write-Text([string]$path, [string]$text) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, $text, $utf8)
}

# 从用例源码里读期望声明：第一行 `/* expect: err */`
#   期望值取值：ok / err / warn:<条数>
function Read-Expect([string]$srcPath) {
    $first = (Get-Content -LiteralPath $srcPath -TotalCount 1)
    if ($null -eq $first) { return "ok" }
    if ($first -match '/\*\s*expect:\s*([A-Za-z]+)(?::(\d+))?\s*\*/') {
        $kind = $Matches[1].ToLowerInvariant()
        $n = $Matches[2]
        if ($kind -eq "warn" -and $n) { return "warn:$n" }
        return $kind
    }
    return "ok"
}

# 从 stderr 里抽出诊断行，去掉 "minic: " 前缀、去掉尾部汇总行
function Get-DiagLines([string]$stderrText) {
    $lines = @()
    foreach ($raw in ((NormalizeNewlines $stderrText) -split "`n")) {
        $t = $raw.Trim()
        if (-not $t) { continue }
        if ($t -match '^\d+\s+(error|warning)\(s\)\s+found') { continue }
        if ($t -match '^minic:\s+(error|warning):') {
            $lines += ($t -replace '^minic:\s+', '')
        }
    }
    return $lines
}

$cases = Get-ChildItem -LiteralPath $caseDir -Filter "*.mc" | Sort-Object Name
if ($Case) {
    $cases = $cases | Where-Object { $_.BaseName -ceq $Case }
    if (-not $cases) {
        Write-Host "FAIL: no such case '$Case' under $caseDir" -ForegroundColor Red
        exit 2
    }
}
if (-not $cases) {
    Write-Host "FAIL: no .mc cases under $caseDir" -ForegroundColor Red
    exit 2
}

# 临时目录：建在仓库内（build\_sema_scratch），避免受限环境下 %TEMP% 不可写
$scratch = Join-Path $repoRoot "build\_sema_scratch"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

$pass = 0
$fail = 0
$updated = 0

foreach ($c in $cases) {
    $name    = $c.BaseName
    $expect  = Read-Expect $c.FullName
    $golden  = Join-Path $caseDir ($name + ".expected.diag.txt")
    $workSrc = Join-Path $scratch ($name + ".mc")
    $outFile = Join-Path $scratch ($name + ".stdout.txt")
    $errFile = Join-Path $scratch ($name + ".stderr.txt")

    Copy-Item -LiteralPath $c.FullName -Destination $workSrc -Force

    $proc = Start-Process -FilePath $Exe `
        -ArgumentList @("`"$workSrc`"") `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $exitCode = $proc.ExitCode
    # 受限环境（Windows 沙箱 / 精简权限）下 Start-Process -PassThru 取不到 .ExitCode，
    # 返回 $null，症状是 `exit code expected 0, got `（got 后为空）。
    # 此时用直接调用重跑一次拿真实退出码（& 调用在当前进程内执行，$LASTEXITCODE 可靠）。
    if ($null -eq $exitCode) {
        & $Exe $workSrc 1>$outFile 2>$errFile | Out-Null
        $exitCode = $LASTEXITCODE
    }

    $diag = Get-DiagLines (Read-Text $errFile)
    $errCount = @($diag | Where-Object { $_ -like 'error:*' }).Count
    $warnCount = @($diag | Where-Object { $_ -like 'warning:*' }).Count
    $actualBlock = ($diag -join "`n")

    if ($Update) {
        Write-Text $golden ($actualBlock.TrimEnd("`n") + "`n")
        Write-Host ("UPDATED  {0}  (expect={1}, exit={2}, err={3}, warn={4})" -f $name, $expect, $exitCode, $errCount, $warnCount) -ForegroundColor Yellow
        $updated++
        continue
    }

    $problems = @()
    if ($expect -eq "err") {
        if ($exitCode -ne 1) { $problems += "expected exit code 1, got $exitCode" }
        if ($errCount -lt 1) { $problems += "expected at least one error, got none" }
    } elseif ($expect -like "warn:*") {
        $want = [int]($expect.Split(':')[1])
        if ($exitCode -ne 0) { $problems += "expected exit code 0, got $exitCode" }
        if ($errCount -ne 0) { $problems += "expected no error, got $errCount" }
        if ($warnCount -ne $want) { $problems += "expected $want warning(s), got $warnCount" }
    } else {
        if ($exitCode -ne 0) { $problems += "expected exit code 0, got $exitCode" }
        if ($errCount -ne 0) { $problems += "expected no error, got $errCount" }
    }

    if (Test-Path -LiteralPath $golden) {
        $expBlock = (NormalizeNewlines (Read-Text $golden)).TrimEnd("`n")
        $actBlock = (NormalizeNewlines $actualBlock).TrimEnd("`n")
        # 错误用例要求逐字一致（含行列号）；ok 用例基线就是空，也要求一致
        if ($expBlock -ne $actBlock) { $problems += "diagnostics mismatch ($name.expected.diag.txt)" }
    } else {
        $problems += "missing golden $name.expected.diag.txt (run with -Update after review)"
    }

    if ($problems.Count -eq 0) {
        Write-Host ("  [ OK ] {0,-20} expect={1,-7} err={2} warn={3}" -f $name, $expect, $errCount, $warnCount) -ForegroundColor Green
        $pass++
    } else {
        Write-Host ("  [FAIL] {0,-20} expect={1,-7} err={2} warn={3}" -f $name, $expect, $errCount, $warnCount) -ForegroundColor Red
        foreach ($p in $problems) { Write-Host ("         - " + $p) -ForegroundColor Red }
        if (Test-Path -LiteralPath $golden) {
            Write-Host "         --- expected ---" -ForegroundColor DarkGray
            ((NormalizeNewlines (Read-Text $golden)).TrimEnd("`n")) -split "`n" | ForEach-Object { Write-Host ("         | " + $_) -ForegroundColor DarkGray }
        }
        Write-Host "         --- actual ---" -ForegroundColor DarkGray
        if ($actualBlock) {
            $actualBlock -split "`n" | ForEach-Object { Write-Host ("         | " + $_) -ForegroundColor DarkGray }
        } else {
            Write-Host "         | (no diagnostics)" -ForegroundColor DarkGray
        }
        $fail++
    }
}

Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
if ($Update) {
    Write-Host ("Sema goldens updated: {0} file(s). 请人工核对刷新后的诊断。" -f $updated) -ForegroundColor Yellow
    if ($fail -gt 0) { exit 1 }
    exit 0
}

Write-Host ("Sema cases: {0} passed, {1} failed (total {2})" -f $pass, $fail, ($pass + $fail))
if ($fail -eq 0) {
    Write-Host "All sema tests passed." -ForegroundColor Green
    exit 0
} else {
    Write-Host "Sema tests FAILED." -ForegroundColor Red
    exit 1
}
