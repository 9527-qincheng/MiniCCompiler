#Requires -Version 7.0
# ============================================================================
# MiniC 编译器 —— 目标代码生成回归运行器
#
# 流程：对 tests\codegen\*.mc 逐个
#   1) minic.exe 生成 .asm（x86-64 / GNU as / Windows MinGW）
#   2) gcc -x assembler 汇编 + 链接成 .exe
#   3) 运行 .exe，把 stdout 与同名 .expected.txt 逐行比对
#
# 期望输出文件命名：tests\codegen\<用例名>.expected.txt（一行一个期望输出）
# 若某用例没有 .expected.txt，只检查「能汇编、能链接、退出码为 0」。
#
# 用法：
#   pwsh -File tests\run_codegen_tests.ps1
#   pwsh -File tests\run_codegen_tests.ps1 -Case while_sum      # 只跑一个
# 需要 g++ 与 gcc 在 PATH 中（可用 -Cxx / -Cc 指定）。
# ============================================================================

param(
    [string]$Cxx = "g++",
    [string]$Cc  = "gcc",
    [string]$Case = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$srcDir    = Join-Path $repoRoot "src"
$caseDir   = Join-Path $scriptDir "codegen"
# 工作目录建在仓库内（build\ 已被 .gitignore 排除），避免系统 %TEMP% 与中文路径问题
$workDir   = Join-Path $repoRoot "build\_codegen_run"

$cxxPath = (Get-Command $Cxx -ErrorAction SilentlyContinue).Source
if (-not $cxxPath) { Write-Host "FAIL: C++ compiler not found: $Cxx" -ForegroundColor Red; exit 2 }
$ccPath  = (Get-Command $Cc  -ErrorAction SilentlyContinue).Source
if (-not $ccPath)  { Write-Host "FAIL: assembler driver not found: $Cc" -ForegroundColor Red; exit 2 }

if (-not (Test-Path $caseDir)) { Write-Host "FAIL: case dir not found: $caseDir" -ForegroundColor Red; exit 2 }

New-Item -ItemType Directory -Force -Path $workDir | Out-Null

# 注意：MinGW 的 ld 不接受含中文的绝对路径作为 -o 目标（会报
# "cannot open output file ... No such file or directory"），因此切到仓库根
# 并统一使用相对路径传给编译/汇编驱动。
Push-Location $repoRoot
$minicRel   = "build\_codegen_run\minic.exe"
$caseRelDir = "tests\codegen"
$workRelDir = "build\_codegen_run"
$minic = $minicRel

# ---- 1. 编译编译器本体 ----------------------------------------------------
$allSources = Get-ChildItem -Path $srcDir -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
Write-Host "Compiling minic.exe ..."
& $cxxPath -std=c++17 -Wall -Wextra -I $srcDir -o $minicRel @allSources
if ($LASTEXITCODE -ne 0) { Pop-Location; Write-Host "FAIL: compiler build error" -ForegroundColor Red; exit 2 }

# ---- 2. 逐用例：生成 -> 汇编 -> 链接 -> 运行 -> 比对 -----------------------
$cases = if ($Case -ne "") { @(Get-ChildItem -Path $caseDir -Filter "$Case.mc") }
         else              { @(Get-ChildItem -Path $caseDir -Filter *.mc) }
if ($cases.Count -eq 0) { Write-Host "FAIL: no test case matched" -ForegroundColor Red; exit 2 }

$pass = 0
$fail = 0
$failedNames = @()

foreach ($mc in $cases) {
    $name = $mc.BaseName
    $asmRel = "$caseRelDir\$name.asm"
    $sRel   = "$workRelDir\$name.s"
    $exeRel = "$workRelDir\$name.exe"

    # 生成汇编
    & $minic $mc.FullName *> $null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $asmRel)) {
        Write-Host "[FAIL] $name : minic exit=$LASTEXITCODE, no .asm" -ForegroundColor Red
        $fail++; $failedNames += $name; continue
    }

    # 汇编 + 链接（.asm 后缀不被 gcc 识别，统一改名为 .s）
    Copy-Item -LiteralPath $asmRel -Destination $sRel -Force
    & $ccPath -x assembler -o $exeRel $sRel *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] $name : assemble/link error" -ForegroundColor Red
        $fail++; $failedNames += $name; continue
    }

    # 运行
    $out = @(& $exeRel 2>&1)
    $rc  = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Host "[FAIL] $name : run exit=$rc" -ForegroundColor Red
        $fail++; $failedNames += $name; continue
    }

    # 与期望输出比对
    $expFile = Join-Path $caseDir "$name.expected.txt"
    if (Test-Path $expFile) {
        $exp = @(Get-Content -LiteralPath $expFile | Where-Object { $_ -ne "" })
        $got = @($out | ForEach-Object { "$_" } | Where-Object { $_ -ne "" })
        $same = ($exp.Count -eq $got.Count)
        if ($same) {
            for ($i = 0; $i -lt $exp.Count; $i++) {
                if ($exp[$i].Trim() -ne $got[$i].Trim()) { $same = $false; break }
            }
        }
        if (-not $same) {
            Write-Host "[FAIL] $name : output mismatch" -ForegroundColor Red
            Write-Host ("   expect: " + ($exp -join " | "))
            Write-Host ("   actual: " + ($got -join " | "))
            $fail++; $failedNames += $name; continue
        }
    }

    Write-Host "[ OK ] $name" -ForegroundColor Green
    $pass++
}

# ---- 3. 汇总 --------------------------------------------------------------
Pop-Location
Write-Host ""
Write-Host ("Codegen cases: {0} passed, {1} failed (total {2})" -f $pass, $fail, ($pass + $fail))
if ($fail -gt 0) {
    Write-Host ("Failed: " + ($failedNames -join ", ")) -ForegroundColor Red
    exit 1
}
Write-Host "All codegen tests passed." -ForegroundColor Green
exit 0
