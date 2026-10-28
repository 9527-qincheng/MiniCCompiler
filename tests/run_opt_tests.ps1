#Requires -Version 7.0
# 说明：tests\*.ps1 以 UTF-8 无 BOM 保存且含中文注释，Windows PowerShell 5.1 会按
#       ANSI 解码并报出位置误导的语法错，故要求 PowerShell 7+（pwsh）。
# ============================================================================
# MiniC 编译器 —— 优化后四元式（-O --dump-quads）回归测试
# 用法：
#   pwsh -File tests\run_opt_tests.ps1
#   pwsh -File tests\run_opt_tests.ps1 -Case licm_hoist
#   pwsh -File tests\run_opt_tests.ps1 -Update
#
# 与 run_quad_tests.ps1 的区别只有一处：编译时多带一个 -O。
# 这样「优化器到底做了什么」就有了逐字基线 —— 默认路径的产物由 quad 那套守着，
# 优化路径的产物由这一套守着，两边互不影响。
#
# 说明：
#   - 遍历 tests\opt\*.mc，逐个在 build\_opt_scratch 里重新编译并 -O --dump-quads，
#     与 tests\opt\<用例>.expected.quad.txt 逐字比对
#   - .quad.txt 是四类必交产物之一，也是「中间代码」这一阶段的唯一可观测输出。
#     没有这层回归，改 IRGenerator 时表达式求值顺序、临时变量编号、标号编号、
#     短路展开形态、类型转换插入位置都可能悄悄漂移，而端到端只比 stdout，
#     这些结构变化完全测不出来
#   - golden 命名刻意用 .expected.quad.txt 而不是 .quad.txt：minic 会把产物写在
#     源文件同目录，若 golden 叫 <用例>.quad.txt，任何人直接对 tests\quad\*.mc
#     跑一次编译就会把基线覆盖掉。放在 .expected.quad.txt 上永远不会被写入
#   - 要求退出码为 0（中间代码只有在前面几段都通过后才会生成）
#   - 另查一条硬约束：产物里不得出现盘符或反斜杠路径。四元式是逻辑表示，
#     一旦混进路径，基线就与机器绑定、无法跨环境比对
#   - `-Update` 用当前输出刷新全部基线；仅在确认变更是有意为之、且已人工核对
#     刷新后的四元式序列时才使用，否则等于自我印证
#   - 写入统一 UTF-8 无 BOM + LF（同 run_lexer_tests.ps1 的 Write-Golden）
# ============================================================================

param(
    [string]$Exe = "",
    [string]$Case = "",
    [switch]$Update
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$caseDir   = Join-Path $scriptDir "opt"

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

# 临时目录：建在仓库内（build\_opt_scratch），避免受限环境下 %TEMP% 不可写
# 导致"假失败"（症状：cannot write output file: ...quad.txt）。
$scratch = Join-Path $repoRoot "build\_opt_scratch"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

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

$pass = 0
$fail = 0
$updated = 0

foreach ($c in $cases) {
    $name     = $c.BaseName
    $golden   = Join-Path $caseDir ($name + ".expected.quad.txt")
    $workSrc  = Join-Path $scratch ($name + ".mc")
    $outFile  = Join-Path $scratch ($name + ".stdout.txt")
    $errFile  = Join-Path $scratch ($name + ".stderr.txt")

    Copy-Item -LiteralPath $c.FullName -Destination $workSrc -Force
    # 清掉可能残留的旧产物，避免把上一轮的输出误当成本轮结果
    $quadPath = [System.IO.Path]::ChangeExtension($workSrc, ".quad.txt")
    Remove-Item -LiteralPath $quadPath -Force -ErrorAction SilentlyContinue

    $proc = Start-Process -FilePath $Exe `
        -ArgumentList @("`"$workSrc`"", "-O", "--dump-quads") `
        -Wait -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $exitCode = $proc.ExitCode
    # 受限环境（Windows 沙箱 / 精简权限）下 Start-Process -PassThru 取不到 .ExitCode，
    # 返回 $null，症状是 `expected exit code 0, got `（got 后为空）。
    # 此时用直接调用重跑一次拿真实退出码（& 调用在当前进程内执行，$LASTEXITCODE 可靠）。
    if ($null -eq $exitCode) {
        & $Exe $workSrc -O --dump-quads 1>$outFile 2>$errFile | Out-Null
        $exitCode = $LASTEXITCODE
    }

    $got = Read-Text $quadPath
    $err = Read-Text $errFile

    if ($Update) {
        if ($null -eq $got) {
            Write-Host "FAIL: [$name] no .quad.txt produced (compile failed?)" -ForegroundColor Red
            Write-Host $err -ForegroundColor Red
            $fail++
            continue
        }
        Write-Golden $golden $got
        Write-Host "UPDATED  $name" -ForegroundColor Yellow
        $updated++
        continue
    }

    $problems = @()
    if ($exitCode -ne 0) {
        $problems += "expected exit code 0, got $exitCode"
    }
    if ($null -eq $got) {
        $problems += "no .quad.txt produced"
    } else {
        $gotN = Normalize $got
        if (Test-Path -LiteralPath $golden) {
            $exp = Normalize (Read-Text $golden)
            if ($exp -ne $gotN) { $problems += "quad dump mismatch ($name.expected.quad.txt)" }
        } else {
            $problems += "missing golden $name.expected.quad.txt (run with -Update after review)"
        }
        if ($gotN -match '[A-Za-z]:[\\/]' -or $gotN -match '\\') {
            $problems += "quad text contains an absolute or backslash path; must stay machine independent"
        }
        if ($gotN -notmatch '// --- proc: ') {
            $problems += "quad text has no '// --- proc:' section header"
        }
    }

    if ($problems.Count -eq 0) {
        Write-Host "  [ OK ] $name" -ForegroundColor Green
        $pass++
    } else {
        Write-Host "  [FAIL] $name" -ForegroundColor Red
        foreach ($p in $problems) { Write-Host ("         - " + $p) -ForegroundColor Red }
        if ($null -ne $got -and (Test-Path -LiteralPath $golden)) {
            Write-Host "         --- expected ---" -ForegroundColor DarkGray
            (Normalize (Read-Text $golden)) -split "`n" | ForEach-Object { Write-Host ("         | " + $_) -ForegroundColor DarkGray }
            Write-Host "         --- actual ---" -ForegroundColor DarkGray
            (Normalize $got) -split "`n" | ForEach-Object { Write-Host ("         | " + $_) -ForegroundColor DarkGray }
        }
        if ($err) {
            Write-Host "         --- stderr ---" -ForegroundColor DarkGray
            Write-Host ("         | " + ($err -replace "`n", "`n         | ")) -ForegroundColor DarkGray
        }
        $fail++
    }
}

Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
if ($Update) {
    Write-Host ("Opt goldens updated: {0} file(s). 请人工核对刷新后的四元式序列。" -f $updated) -ForegroundColor Yellow
    if ($fail -gt 0) { exit 1 }
    exit 0
}

Write-Host ("Opt cases: {0} passed, {1} failed (total {2})" -f $pass, $fail, ($pass + $fail))
if ($fail -eq 0) {
    Write-Host "All opt tests passed." -ForegroundColor Green
    exit 0
} else {
    Write-Host "Opt tests FAILED." -ForegroundColor Red
    exit 1
}
