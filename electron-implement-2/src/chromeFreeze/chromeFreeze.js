const { spawn } = require('node:child_process');
const readline = require('node:readline');
const path = require('node:path');

// One hidden PowerShell process lives for the whole app session. The slow part
// (Add-Type compiling the P/Invoke declarations, ~0.3-1s) happens exactly once.
// After that a freeze/unfreeze is one line written to stdin and one line read
// back, so each call is only a few milliseconds.
//
// No native Node module, no helper .exe: the P/Invoke code below is C# text
// passed to PowerShell's built-in Add-Type -MemberDefinition.
const PS_SCRIPT = String.raw`
$ErrorActionPreference = 'SilentlyContinue'

try {
  Add-Type -Namespace Win32 -Name Native -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
[DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
[DllImport("ntdll.dll")] public static extern int NtSuspendProcess(IntPtr handle);
[DllImport("ntdll.dll")] public static extern int NtResumeProcess(IntPtr handle);
'@
} catch {
  # P/Invoke unavailable: the loop still answers, so Node never hangs.
}

$processName = 'chrome'
$held = @{}   # pid -> start time ticks (guards against PID reuse)

function Invoke-Nt {
  param([int]$procId, [bool]$suspend)

  try {
    $handle = [Win32.Native]::OpenProcess(0x0800, $false, [uint32]$procId)
    if ($handle -eq [IntPtr]::Zero) { return $false }
    try {
      if ($suspend) { $status = [Win32.Native]::NtSuspendProcess($handle) }
      else { $status = [Win32.Native]::NtResumeProcess($handle) }
      return ($status -ge 0)   # NT_SUCCESS(status) is status >= 0
    } finally {
      [void][Win32.Native]::CloseHandle($handle)
    }
  } catch {
    return $false
  }
}

# Idempotent: also catches chrome child processes spawned since the last call.
function Freeze {
  $count = 0
  foreach ($p in [Diagnostics.Process]::GetProcessesByName($processName)) {
    try {
      $id = $p.Id
      $started = $p.StartTime.Ticks
      if ($held.ContainsKey($id) -and $held[$id] -eq $started) { continue }
      if (Invoke-Nt $id $true) {
        $held[$id] = $started
        $count++
      }
    } catch {} finally {
      if ($null -ne $p) { $p.Dispose() }
    }
  }
  return $count
}

# Resume only the processes this helper froze, and only when pid, start time
# and process name all still match. A recycled pid is never touched.
function Unfreeze {
  $count = 0
  foreach ($id in @($held.Keys)) {
    $p = $null
    try {
      $p = [Diagnostics.Process]::GetProcessById($id)
      if (($p.StartTime.Ticks -eq $held[$id]) -and ($p.ProcessName -eq $processName)) {
        if (Invoke-Nt $id $false) { $count++ }
      }
    } catch {} finally {
      if ($null -ne $p) { $p.Dispose() }
    }
  }
  $held.Clear()
  return $count
}

# Startup recovery after a crash of the previous session: resume every chrome
# process. Resuming a process that is not suspended is harmless.
function ResumeAll {
  $count = 0
  foreach ($p in [Diagnostics.Process]::GetProcessesByName($processName)) {
    try {
      if (Invoke-Nt $p.Id $false) { $count++ }
    } catch {} finally {
      if ($null -ne $p) { $p.Dispose() }
    }
  }
  $held.Clear()
  return $count
}

while ($null -ne ($line = [Console]::In.ReadLine())) {
  $parts = $line.Trim().Split(' ')
  if ($parts.Length -lt 2) { continue }

  switch ($parts[1]) {
    'F' { $result = Freeze }
    'U' { $result = Unfreeze }
    'R' { $result = ResumeAll }
    default { $result = -1 }
  }

  # The reader may already be gone during app shutdown; never let a broken
  # stdout stop us from reaching the cleanup below.
  try { [Console]::Out.WriteLine("$($parts[0]) $result") } catch {}
}

# stdin closed == Electron quit, crash, or Task Manager kill. Either way, give
# Chrome back. This runs inside PowerShell, so it does not need Node to live.
[void](Unfreeze)
`;

const DEFAULT_TIMEOUT_MS = 5000;
const STARTUP_TIMEOUT_MS = 15000;

let child = null;
let reader = null;
let shuttingDown = false;
let seq = 0;
const pending = new Map();

function settlePending() {
  for (const resolve of pending.values()) resolve(0);
  pending.clear();
}

function onChildExit() {
  child = null;
  if (reader) {
    reader.close();
    reader = null;
  }
  settlePending();
  // Do not respawn here. The next command calls start(), and start() always
  // sends 'R' first, which resumes anything frozen when the helper died.
}

function powerShellPath() {
  return path.join(
    process.env.SystemRoot || 'C:\\Windows',
    'System32',
    'WindowsPowerShell',
    'v1.0',
    'powershell.exe',
  );
}

function start() {
  if (process.platform !== 'win32' || child || shuttingDown) return;

  const encoded = Buffer.from(PS_SCRIPT, 'utf16le').toString('base64');

  try {
    child = spawn(
      powerShellPath(),
      [
        '-NoLogo',
        '-NoProfile',
        '-NonInteractive',
        '-ExecutionPolicy',
        'Bypass',
        '-EncodedCommand',
        encoded,
      ],
      { windowsHide: true, stdio: ['pipe', 'pipe', 'ignore'] },
    );
  } catch {
    child = null;
    return;
  }

  child.stdin.on('error', () => {});
  child.on('error', onChildExit);
  child.on('exit', onChildExit);

  reader = readline.createInterface({ input: child.stdout });
  reader.on('line', (line) => {
    const [rawId, rawValue] = line.trim().split(' ');
    const id = Number(rawId);
    const resolve = pending.get(id);
    if (!resolve) return;
    pending.delete(id);
    resolve(Number(rawValue) || 0);
  });

  // Recovery runs once per helper, queued ahead of every other command.
  writeCommand('R', STARTUP_TIMEOUT_MS);
}

function writeCommand(cmd, timeoutMs = DEFAULT_TIMEOUT_MS) {
  start();
  if (!child || !child.stdin || child.stdin.destroyed) return Promise.resolve(0);

  const id = ++seq;

  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      if (!pending.delete(id)) return;
      // Helper is stuck (or was killed): drop it. The next call starts a fresh
      // one, whose 'R' command resumes everything before doing new work.
      const stuck = child;
      child = null;
      settlePending();
      if (stuck && !stuck.killed) {
        try {
          stuck.kill();
        } catch {
          // Nothing else we can do.
        }
      }
      resolve(0);
    }, timeoutMs);
    timer.unref?.();

    pending.set(id, (value) => {
      clearTimeout(timer);
      resolve(value);
    });

    try {
      child.stdin.write(`${id} ${cmd}\n`);
    } catch {
      clearTimeout(timer);
      pending.delete(id);
      resolve(0);
    }
  });
}

// Start the one-time PowerShell/Add-Type initialization. Safe to call early:
// commands written before init finishes are simply buffered in the pipe.
function initializeChromeFreeze() {
  start();
}

async function freezeChrome() {
  if (shuttingDown) return 0;
  return writeCommand('F');
}

async function unfreezeChrome() {
  if (shuttingDown) return 0;
  return writeCommand('U');
}

// Ending stdin makes the helper run Unfreeze itself. That covers a normal quit
// as well as a crash or kill of Electron, where no Node code can run anymore.
function shutdownChromeFreeze() {
  if (shuttingDown) return;

  if (child) writeCommand('U', 1000);

  shuttingDown = true;

  if (child) {
    try {
      child.stdin.end();
    } catch {
      // EOF happens when the process exits anyway.
    }
  }
}

module.exports = {
  initializeChromeFreeze,
  freezeChrome,
  unfreezeChrome,
  shutdownChromeFreeze,
};
