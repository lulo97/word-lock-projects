# src/tests/smoke.ps1
#
# The ONE real-process smoke test. Its only job: prove app.exe starts and the
# tray icon registers. Everything else is covered by headless feature tests
# (src/tests/features/*) and engine tests (src/tests/cases/*), which need no launched
# process.
#
# Usage:  powershell -ExecutionPolicy Bypass -File src\tests\smoke.ps1
# Exit:    0 if the app launched and its state is readable, 1 otherwise.

$ErrorActionPreference = "Stop"
$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # project root
$exe     = Join-Path $root "build\app.exe"
$cmdFile = Join-Path $root "debug_cmd.txt"
$outFile = Join-Path $root "debug_out.txt"
$totalSw = [Diagnostics.Stopwatch]::StartNew()

function Log([string]$msg) {
    Write-Host ("  [{0,6:n2}s] {1}" -f $totalSw.Elapsed.TotalSeconds, $msg) -ForegroundColor DarkGray
}

function Unfreeze {
    # Thaw any Chrome left suspended. Launched hidden and waited on with a
    # timeout, never piped, so it cannot hang the script.
    if (-not (Test-Path $exe)) { return }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList "--unfreeze" `
         -WindowStyle Hidden -PassThru
    if (-not $p.WaitForExit(8000)) { try { $p.Kill() } catch {} }
    Log ("unfreeze took {0:n2}s" -f $sw.Elapsed.TotalSeconds)
}

function Get-AppState([int]$timeoutMs = 5000) {
    # Ask the running app for its state and wait (polling) for the answer.
    Remove-Item $outFile -ErrorAction SilentlyContinue
    Set-Content -Path $cmdFile -Value "app state" -Encoding ASCII
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        if ($proc.HasExited) { throw "app.exe exited (code $($proc.ExitCode))" }
        if (Test-Path $outFile) {
            try {
                $txt = Get-Content $outFile -Raw
                if ($txt -and $txt -match '"countdown"') { return $txt }
            } catch { }   # file still being written; retry
        }
        Start-Sleep -Milliseconds 25
    }
    throw "debug_out.txt not produced within ${timeoutMs}ms; app did not poll the command file"
}

if (-not (Test-Path $exe)) {
    Write-Host "app.exe not found at $exe (run build.bat first)"
    exit 1
}

# Clean up stale instances and generated files first.
Get-Process -Name app -ErrorAction SilentlyContinue | Stop-Process -Force
Unfreeze
Remove-Item $cmdFile -ErrorAction SilentlyContinue
Remove-Item $outFile -ErrorAction SilentlyContinue

$ok = $false
$proc = $null
try {
    # Launch in debug mode (it polls debug_cmd.txt so we can query state).
    # Working directory is pinned so the debug files land in the project root.
    $proc = Start-Process -FilePath $exe -ArgumentList "--debug" `
            -WorkingDirectory $root -PassThru
    Log "app launched (pid $($proc.Id))"

    # First poll: waits as long as the app needs to create windows + tray.
    $state = Get-AppState 8000
    Log "first state received"
    Write-Host "app state: $state"

    if ($state -notmatch '"stopped":false') {
        throw "app state missing 'stopped:false' (tray/countdown not running)"
    }

    # Prove the countdown timer is live: it must decrease between polls
    # (debug mode ticks at 10 ms). If it already completed, accept the
    # post-countdown state: countdown=0 plus the input window.
    $c1 = $null
    if ($state -match '"countdown":(\d+)') { $c1 = [int]$Matches[1] }
    if ($null -eq $c1) { throw "no countdown value in state" }

    $live = $false
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt 3000) {
        $state2 = Get-AppState 3000
        $c2 = $null
        if ($state2 -match '"countdown":(\d+)') { $c2 = [int]$Matches[1] }
        $completed = ($c1 -eq 0) -and ($state2 -match '"inputWin":true')
        if ($null -ne $c2 -and (($c2 -lt $c1) -or $completed)) {
            $live = $true
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $live) {
        throw "countdown not decreasing between polls (c1=$c1 c2=$c2); timer not live"
    }
    Log "countdown live: $c1 -> $c2"
    $ok = $true
}
catch {
    Write-Host "SMOKE FAIL: $($_.Exception.Message)"
}
finally {
    # Always clean up. Order matters: kill the app first (Stop-Process does
    # not run its own cleanup), THEN thaw Chrome.
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item $cmdFile -ErrorAction SilentlyContinue
    Remove-Item $outFile -ErrorAction SilentlyContinue
    Unfreeze
}

if ($ok) {
    Write-Host "SMOKE OK: app.exe launched, tray registered, state readable"
    exit 0
}
exit 1