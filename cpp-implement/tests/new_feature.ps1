# tests/new_feature.ps1
#
# Scaffold a new feature test: copies the checklist template into
# tests/features/<name>/steps.txt and prints the command to watch it fail
# first. Every feature test then has the same shape; the checklist stops
# living only in your head.
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests\new_feature.ps1 tray_open
#
# For bug regressions, use the same shape under tests\regress\<bug_name>\
# steps.txt (mirrors the "one test per bug" unit convention).

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Name
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if ($Name -notmatch '^[a-z0-9_]+$') {
    Write-Error "name must be lowercase letters/digits/underscore: $Name"
    exit 1
}
$dir = Join-Path $root "tests\features\$Name"
if (Test-Path $dir) {
    Write-Error "tests\features\$Name already exists"
    exit 1
}
New-Item -ItemType Directory -Force $dir | Out-Null

$template = @'
# <feature> test.
#
# One folder per feature; this steps.txt drives app_test.exe headless
# (no launched process, no real timers). It must FAIL before the change
# and PASS after.
#
# Per-feature checklist:
#   1. happy path   - do the action, assert state and UI
#   2. reverse      - undo/toggle back, assert you return to the start state
#   3. repeat       - do the action twice (catches double-timer / leaked
#                      window bugs)
#   4. interleaving - do it mid-countdown, while the input window is
#                      showing, while the info window is showing
#   5. side effects - timers are dead (app assert timers == 0), no extra
#                      windows, hooks recorded (app assert freezeCalls == N)

# --- setup: app is running, word pinned (shared preamble) ---
include ../common/running.txt

# --- 1. happy path ---

# --- 2. reverse ---

# --- 3. repeat ---

# --- 4. interleaving ---

# --- 5. side effects ---
app assert timers == 0
'@

$steps = Join-Path $dir "steps.txt"
Set-Content $steps ($template -replace "<feature>", $Name)

Write-Host "Created $steps"
Write-Host ""
Write-Host "Implement the feature, then watch the test fail first:"
Write-Host "  powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Filter $Name"
