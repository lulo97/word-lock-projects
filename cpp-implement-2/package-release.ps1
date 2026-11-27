$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
Set-Location $root

Write-Host 'Building Release...'
& cmd.exe /c 'build.bat'
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$source = Join-Path $root 'build\Release'
$releaseRoot = Join-Path $root 'release'
$packageDir = Join-Path $releaseRoot 'PomodoroLockCpp'

if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# Everything at the top level of build\Release is an app asset or runtime DLL.
# WebView2 user-data folders are intentionally excluded.
Get-ChildItem -LiteralPath $source -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packageDir -Force
}

Copy-Item -LiteralPath (Join-Path $source 'mediapipe') `
          -Destination (Join-Path $packageDir 'mediapipe') -Recurse -Force

# MSVC runtime DLLs are bundled so the target PC does not need a C++ redistributable.
foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    $systemDll = Join-Path $env:WINDIR "System32\$dll"
    if (Test-Path -LiteralPath $systemDll) {
        Copy-Item -LiteralPath $systemDll -Destination $packageDir -Force
    } else {
        Write-Warning "$dll was not found in System32. The target PC may need the VC++ 2015-2022 x64 redistributable."
    }
}

$required = @(
    'PomodoroLockCpp.exe',
    'WebView2Loader.dll',
    'sqlite3.dll',
    'pose_landmarker_heavy.task',
    'mediapipe\vision_bundle.mjs',
    'mediapipe\wasm\vision_wasm_internal.wasm',
    'msvcp140.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll'
)
foreach ($relative in $required) {
    $candidate = Join-Path $packageDir $relative
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Missing required release file: $relative"
    }
}

$readme = @'
PomodoroLockCpp - Windows 11 x64
================================

1. Keep ALL files in this folder together.
2. Run PomodoroLockCpp.exe.
3. No C++ compiler, Python, Node.js or npm is needed on this computer.

Requirements:
- Microsoft Edge WebView2 Runtime. It is preinstalled on most Windows 11 PCs.
  If WebView2 fails to start, install "Microsoft Edge WebView2 Runtime"
  (Evergreen Runtime Bootstrapper, x64) and run the app again.
- A webcam if you want to use the dumbbell workout. Allow camera access when
  Windows asks. Dumbbell pages are served from https://app/, a secure origin.

Do not copy only PomodoroLockCpp.exe: the HTML/CSS/JS, MediaPipe WebAssembly,
pose_landmarker_heavy.task and DLLs must stay next to the executable.
'@
Set-Content -LiteralPath (Join-Path $packageDir 'README.txt') -Value $readme -Encoding UTF8

$zip = Join-Path $releaseRoot 'PomodoroLockCpp-win11-x64.zip'
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path $packageDir -DestinationPath $zip -CompressionLevel Optimal

Write-Host ''
Write-Host "Release folder: $packageDir"
Write-Host "Release zip:    $zip"
Write-Host 'Copy the whole folder, or extract the zip, on your friend''s Windows 11 PC.'
