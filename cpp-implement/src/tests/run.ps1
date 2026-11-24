# src/tests/run.ps1
#
# The single test driver: discovers every test, runs it, times it, and prints
# a failure summary plus a timing summary at the end.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1                 # everything
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Filter stop     # fast inner loop
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Update          # re-record goldens
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Seed 42         # fixed fuzz seed
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -Seed random     # nightly fuzz
#   powershell -ExecutionPolicy Bypass -File src\tests\run.ps1 -FuzzIters 300   # quicker fuzz
#   pwsh -File src\tests\run.ps1 -Parallel                                      # parallel case/dpi (PowerShell 7+)
#
#   -Filter     only tests whose path matches the pattern (substring).
#   -Update     sets UPDATE_GOLDEN=1 so `golden` rewrites the baseline.
#   -Seed       fuzz seed, or "random".
#   -FuzzIters  fuzz iterations (default 2000).
#   -Parallel   run case + dpi-matrix tests in parallel (needs PowerShell 7+).
#               Feature tests, fuzz and smoke always stay serial.
#   -Top        how many slowest tests to list in the timing summary (default 10).
#
# Exit: 0 only if every test passes and every script/exe exists.
# Reports: src\tests\out\report.txt (full output + timings)
#          src\tests\out\timings.csv (one row per test, for tracking over time)

param(
    [string]$Filter = "",
    [switch]$Update,
    [string]$Seed = "1",
    [int]$FuzzIters = 2000,
    [switch]$Parallel,
    [int]$Top = 10
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # project root
Set-Location $root                                              # exes read src/tests/word.json etc. from CWD

if ($Update) { $env:UPDATE_GOLDEN = "1" } else { Remove-Item Env:\UPDATE_GOLDEN -ErrorAction SilentlyContinue }
if ($Seed -eq "random") { $Seed = (Get-Random -Minimum 1 -Maximum 2147483647).ToString() }

$canParallel = $Parallel -and ($PSVersionTable.PSVersion.Major -ge 7)
if ($Parallel -and -not $canParallel) {
    Write-Host "NOTE: -Parallel needs PowerShell 7+ (pwsh); running serially." -ForegroundColor Yellow
}

$reportDir = Join-Path $root "src\tests\out"
New-Item -ItemType Directory -Force $reportDir | Out-Null
$report     = Join-Path $reportDir "report.txt"
$timingsCsv = Join-Path $reportDir "timings.csv"
[IO.File]::WriteAllText($report, "")

$appExe = Join-Path $root "build\app.exe"

$script:failed  = @()
$script:timings = New-Object System.Collections.Generic.List[object]   # Name, Seconds, Status
$script:phases  = New-Object System.Collections.Generic.List[object]   # Phase, Seconds, Count
$totalSw = [Diagnostics.Stopwatch]::StartNew()

# ---------------------------------------------------------------- helpers

function Unfreeze {
    if (-not (Test-Path $appExe)) { return }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $appExe -ArgumentList "--unfreeze" `
         -WindowStyle Hidden -PassThru
    if (-not $p.WaitForExit(8000)) { try { $p.Kill() } catch {} }
    Write-Host ("  (unfreeze {0:n2}s)" -f $sw.Elapsed.TotalSeconds) -ForegroundColor DarkGray
}

function Write-Report([string]$text) {
    # One file write instead of one per line.
    [IO.File]::AppendAllText($report, $text + [Environment]::NewLine)
}

function Add-Result([string]$name, [double]$seconds, [bool]$ok) {
    $status = if ($ok) { "PASS" } else { "FAIL" }
    $script:timings.Add([pscustomobject]@{ Name = $name; Seconds = $seconds; Status = $status })
    if (-not $ok) { $script:failed += $name }
    $color = if ($ok) { "Green" } else { "Red" }
    Write-Host ("[{0,7:n2}s] {1,-4} {2}" -f $seconds, $status, $name) -ForegroundColor $color
}

function Invoke-Test($name, $exe, $exeArgs) {
    # One test: run $exe, time it, append output to the report, record result.
    # Missing exe => recorded failure.
    if (-not (Test-Path $exe)) {
        Write-Report "=== $name ===`n  missing exe: $exe"
        Add-Result $name 0 $false
        return
    }

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"      # native stderr must not throw under "Stop"
    $code = 1
    try {
        $out = & $exe @exeArgs 2>&1 | ForEach-Object { "$_" }
        $code = $LASTEXITCODE
    }
    catch {
        $out = @("  runner exception: $($_.Exception.Message)")
    }
    finally {
        $ErrorActionPreference = $prevEap
    }
    $sw.Stop()

    Write-Report ("=== $name ===  ({0:n2}s)`n" -f $sw.Elapsed.TotalSeconds + ($out -join "`n"))
    Add-Result $name $sw.Elapsed.TotalSeconds ($code -eq 0)

    # A failed/hung freeze test must never leave Chrome frozen for later tests.
    if ($name -like "*chrome_freeze*") { Unfreeze }
}

function Match-Filter($path) {
    if ($Filter -eq "") { return $true }
    return ($path -like "*$Filter*")
}

function Measure-Phase([string]$phase, [scriptblock]$body) {
    $before = $script:timings.Count
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $body
    $sw.Stop()
    $script:phases.Add([pscustomobject]@{
        Phase   = $phase
        Seconds = $sw.Elapsed.TotalSeconds
        Count   = $script:timings.Count - $before
    })
}

function Invoke-ParallelTests($jobs) {
    # $jobs: objects with Name, Exe, Args. Runs concurrently (PowerShell 7+),
    # then records results + report output serially, in the original order.
    $results = $jobs | ForEach-Object -Parallel {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $ErrorActionPreference = "Continue"
        $exe = $_.Exe; $a = $_.Args
        if (-not (Test-Path $exe)) {
            [pscustomobject]@{ Name = $_.Name; Code = 1; Secs = 0; Out = @("  missing exe: $exe") }
        } else {
            $out = & $exe @a 2>&1 | ForEach-Object { "$_" }
            [pscustomobject]@{ Name = $_.Name; Code = $LASTEXITCODE; Secs = $sw.Elapsed.TotalSeconds; Out = $out }
        }
    } -ThrottleLimit ([Math]::Max(2, [Environment]::ProcessorCount - 1))

    foreach ($j in $jobs) {
        $r = $results | Where-Object { $_.Name -eq $j.Name } | Select-Object -First 1
        Write-Report ("=== $($r.Name) ===  ({0:n2}s)`n" -f $r.Secs + ($r.Out -join "`n"))
        Add-Result $r.Name $r.Secs ($r.Code -eq 0)
    }
}

# ---------------------------------------------------------------- main

Unfreeze    # thaw leftovers from any previous crashed/killed run

try {
    # ---- discover ----
    $caseScripts    = @(Get-ChildItem "src\tests\cases\*\steps*.txt"   | Sort-Object FullName)
    $featureScripts = @(Get-ChildItem "src\tests\features\*\steps.txt" | Sort-Object FullName)

    # An empty discovery is a failure, not a silent pass.
    if ($caseScripts.Count -eq 0)    { $script:failed += "discovery: no src\tests\cases\*\steps*.txt" }
    if ($featureScripts.Count -eq 0) { $script:failed += "discovery: no src\tests\features\*\steps.txt" }

    $word = "src\tests\word.json"

    # ---- unit tests ----
    if (Match-Filter "unit") {
        Measure-Phase "unit" {
            Invoke-Test "unit tests" "build\unit_tests.exe" @()
        }
    }

    # ---- case scripts (steps*.txt: steps.txt, steps_dpi1.txt, ...) ----
    Measure-Phase "cases" {
        $jobs = @()
        foreach ($s in $caseScripts) {
            if (Match-Filter $s.FullName) {
                $jobs += [pscustomobject]@{
                    Name = "case " + $s.Directory.Name + " " + $s.Name
                    Exe  = (Join-Path $root "build\ui_test.exe")
                    Args = @("--word", $word, "--script", $s.FullName)
                }
            }
        }
        if ($canParallel -and $jobs.Count -gt 1) { Invoke-ParallelTests $jobs }
        else { foreach ($j in $jobs) { Invoke-Test $j.Name $j.Exe $j.Args } }
    }

    # ---- metamorphic DPI matrix, per case page ----
    Measure-Phase "dpi matrix" {
        $jobs = @()
        foreach ($s in $caseScripts) {
            $page = Join-Path $s.Directory.FullName "page.html"
            if ((Test-Path $page) -and (Match-Filter $s.FullName)) {
                $jobs += [pscustomobject]@{
                    Name = "dpi matrix " + $s.Directory.Name
                    Exe  = (Join-Path $root "build\ui_test.exe")
                    Args = @("--word", $word, "--dpi-matrix", $page)
                }
            }
        }
        if ($canParallel -and $jobs.Count -gt 1) { Invoke-ParallelTests $jobs }
        else { foreach ($j in $jobs) { Invoke-Test $j.Name $j.Exe $j.Args } }
    }

    # ---- feature tests: one folder per feature (always serial: real windows/tray/Chrome) ----
    Measure-Phase "features" {
        foreach ($s in $featureScripts) {
            if (Match-Filter $s.FullName) {
                Invoke-Test ("feature " + $s.Directory.Name) "build\app_test.exe" `
                    @("--script", $s.FullName)
            }
        }
    }

    # ---- fuzz (fixed seed for gating, random for nightly) ----
    if (Match-Filter "fuzz") {
        Measure-Phase "fuzz" {
            Invoke-Test "fuzz (seed $Seed, $FuzzIters iters)" "build\fuzz.exe" `
                @("--seed", $Seed, "--iters", "$FuzzIters")
        }
    }

    # ---- the ONE real-process smoke test ----
    if (Match-Filter "smoke") {
        Measure-Phase "smoke" {
            $sw = [Diagnostics.Stopwatch]::StartNew()
            $prevEap = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            $code = 1
            try {
                $out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "src\tests\smoke.ps1") 2>&1 | ForEach-Object { "$_" }
                $code = $LASTEXITCODE
            }
            catch { $out = @("  runner exception: $($_.Exception.Message)") }
            finally { $ErrorActionPreference = $prevEap }
            $sw.Stop()
            Write-Report ("=== smoke ===  ({0:n2}s)`n" -f $sw.Elapsed.TotalSeconds + ($out -join "`n"))
            Add-Result "smoke" $sw.Elapsed.TotalSeconds ($code -eq 0)
        }
    }
}
catch {
    # Unexpected runner error: record it, still fall through to summary + unfreeze.
    $script:failed += "runner error: $($_.Exception.Message)"
    Write-Report "RUNNER ERROR: $($_.Exception.Message)`n$($_.ScriptStackTrace)"
}
finally {
    Unfreeze    # always runs: error, Ctrl+C, or normal finish
}

$totalSw.Stop()

# ---------------------------------------------------------------- summary

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("")
[void]$sb.AppendLine("=== TIMING BY PHASE ===")
foreach ($p in $script:phases) {
    [void]$sb.AppendLine(("  {0,-12} {1,8:n2}s   ({2} tests)" -f $p.Phase, $p.Seconds, $p.Count))
}

[void]$sb.AppendLine("")
[void]$sb.AppendLine("=== SLOWEST $Top TESTS ===")
$script:timings | Sort-Object Seconds -Descending | Select-Object -First $Top | ForEach-Object {
    [void]$sb.AppendLine(("  {0,8:n2}s  {1,-4}  {2}" -f $_.Seconds, $_.Status, $_.Name))
}

$sumSecs = ($script:timings | Measure-Object Seconds -Sum).Sum
[void]$sb.AppendLine("")
[void]$sb.AppendLine(("Tests: {0}   sum of test time: {1:n2}s   wall clock: {2:n2}s" -f `
    $script:timings.Count, $sumSecs, $totalSw.Elapsed.TotalSeconds))

if ($script:failed.Count -eq 0) {
    [void]$sb.AppendLine("=== ALL PASS ===")
} else {
    [void]$sb.AppendLine("=== FAILED: $($script:failed.Count) ===")
    $script:failed | ForEach-Object { [void]$sb.AppendLine("  $_") }
}

Write-Report $sb.ToString()
$script:timings | Export-Csv -NoTypeInformation -Path $timingsCsv

Get-Content $report | Select-Object -Last 40    # failures + timings, no scrolling
Write-Host "Full report: $report"
Write-Host "Timings CSV: $timingsCsv"

exit $(if ($script:failed.Count) { 1 } else { 0 })