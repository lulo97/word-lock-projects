# tests/smoke.ps1
#
# The ONE real-process smoke test. Its only job: prove app.exe starts and the
# tray icon registers. Everything else is covered by headless feature tests
# (tests/features/*) and engine tests (tests/cases/*), which need no launched
# process.
#
# Written in PowerShell (not batch) to avoid the `timeout` PATH collision and
# working-directory problems. Uses Start-Process / Start-Sleep / Stop-Process.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests\smoke.ps1
# Exit:    0 if the app launched and its state is readable, 1 otherwise.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot        # project root (tests\.. )
$exe  = Join-Path $root "build\app.exe"

# Clean up stale instances and generated files first.
Get-Process -Name app -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item (Join-Path $root "debug_cmd.txt") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $root "debug_out.txt") -ErrorAction SilentlyContinue

if (-not (Test-Path $exe)) {
    Write-Error "app.exe not found at $exe (run build.bat first)"
    exit 1
}

# Launch the app in debug mode (it polls debug_cmd.txt so we can query state).
# Pin the working directory so the debug files land in the project root no
# matter where this script is invoked from.
$proc = Start-Process -FilePath $exe -ArgumentList "--debug" `
         -WorkingDirectory $root -PassThru

try {
    # Give it time to create its windows + register the tray icon.
    Start-Sleep -Seconds 2

    if ($proc.HasExited) {
        Write-Error "app.exe exited immediately (code $($proc.ExitCode))"
        exit 1
    }

    # Query the running app's state through its debug command file.
    Set-Content -Path (Join-Path $root "debug_cmd.txt") -Value "app state" -Encoding ASCII
    Start-Sleep -Milliseconds 750

    $outFile = Join-Path $root "debug_out.txt"
    if (-not (Test-Path $outFile)) {
        Write-Error "debug_out.txt not produced; app did not poll the command file"
        exit 1
    }
    $state = Get-Content $outFile -Raw
    Write-Host "app state: $state"

    if ($state -notmatch "stopped=0") {
        Write-Error "app state missing 'stopped=0' (tray/countdown not running)"
        exit 1
    }
}
finally {
    # Always clean up: kill the app and remove the generated files.
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item (Join-Path $root "debug_cmd.txt") -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $root "debug_out.txt") -ErrorAction SilentlyContinue
}

Write-Host "SMOKE OK: app.exe launched, tray registered, state readable"
exit 0
