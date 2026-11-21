/**
 * Freeze and resume Google Chrome / Chromium processes while the word gate is
 * visible, mirroring the Python app's psutil-based ChromeBlocker.
 *
 * Windows has no direct Node API for suspending a process without a native
 * addon, so this module drives ntdll's NtSuspendProcess / NtResumeProcess.
 *
 * IMPORTANT: spawning a fresh `powershell.exe` and compiling the P/Invoke
 * wrapper with Add-Type takes ~1.8 s on a cold process. Doing that on every
 * freeze is what made Chrome keep playing for a second after the gate opened.
 * Instead one PowerShell process is started when the app launches ("warmup")
 * and reused; afterwards each freeze/unfreeze is just a few lines written to
 * its stdin, which takes ~5-20 ms like psutil's proc.suspend().
 */

const { spawn, execFile, spawnSync } = require("child_process");
const path = require("path");

const IS_WINDOWS = process.platform === "win32";
const CREATE_NO_WINDOW = 0x08000000;

function powershellPath() {
  const root = process.env.SystemRoot || process.env.windir || "C:\\Windows";
  return path.join(
    root,
    "System32",
    "WindowsPowerShell",
    "v1.0",
    "powershell.exe",
  );
}

/* ------------------------------------------------------------------ *
 * Long-lived worker: Add-Type once, then answer commands on stdin.
 * ------------------------------------------------------------------ */

const WORKER_SCRIPT = [
  "$ErrorActionPreference = 'SilentlyContinue'",
  "$cs = @'",
  "using System;",
  "using System.Runtime.InteropServices;",
  "public static class KotobaProc {",
  '  [DllImport("ntdll.dll")] public static extern int NtSuspendProcess(IntPtr h);',
  '  [DllImport("ntdll.dll")] public static extern int NtResumeProcess(IntPtr h);',
  "}",
  "'@",
  "Add-Type -TypeDefinition $cs",
  "# Warm the process enumerator too, so the first SUSPEND is not slower.",
  "$null = (Get-Process).Count",
  "$out = [Console]::Out",
  "$in  = [Console]::In",
  "$out.WriteLine('READY')",
  "$out.Flush()",
  "while ($true) {",
  "  $line = $in.ReadLine()",
  "  if ($null -eq $line) { break }",
  "  $line = $line.Trim()",
  "  if ($line -eq '') { continue }",
  "  if ($line -eq 'EXIT') { break }",
  "  if ($line -eq 'SUSPEND') {",
  "    $ids = New-Object System.Collections.ArrayList",
  "    foreach ($p in @(Get-Process)) {",
  "      if ($p.ProcessName -match '^(chrome|chromium|googlechrome)') {",
  "        try {",
  "          [KotobaProc]::NtSuspendProcess($p.Handle) | Out-Null",
  "          [void]$ids.Add($p.Id)",
  "        } catch { }",
  "      }",
  "    }",
  "    $out.WriteLine('OK ' + (($ids | ForEach-Object { $_ }) -join ','))",
  "    $out.Flush()",
  "  } elseif ($line -like 'RESUME*') {",
  "    $rest = $line.Substring(6).Trim()",
  "    foreach ($id in ($rest -split ',')) {",
  "      if ($id -match '^\\d+$') {",
  "        $p = Get-Process -Id ([int]$id) -ErrorAction SilentlyContinue",
  "        if ($p) {",
  "          try { [KotobaProc]::NtResumeProcess($p.Handle) | Out-Null } catch { }",
  "        }",
  "      }",
  "    }",
  "    $out.WriteLine('OK')",
  "    $out.Flush()",
  "  } else {",
  "    $out.WriteLine('OK')",
  "    $out.Flush()",
  "  }",
  "}",
].join("\n");

class PowerShellWorker {
  constructor() {
    this.proc = null;
    this.ready = false;
    this.dead = false;
    this.pending = [];
    this.buffer = "";
    this.startPromise = null;
    this.finishStart = () => {};
  }

  /** Start (or return the already running) worker. Resolves to true when warm. */
  start() {
    if (this.startPromise) return this.startPromise;

    this.startPromise = new Promise((resolve) => {
      let settled = false;
      this.finishStart = (ok) => {
        if (settled) return;
        settled = true;
        resolve(ok);
      };

      try {
        this.proc = spawn(
          powershellPath(),
          [
            "-NoProfile",
            "-NonInteractive",
            "-WindowStyle",
            "Hidden",
            "-ExecutionPolicy",
            "Bypass",
            "-EncodedCommand",
            Buffer.from(WORKER_SCRIPT, "utf16le").toString("base64"),
          ],
          {
            windowsHide: true,
            stdio: ["pipe", "pipe", "ignore"],
            creationFlags: CREATE_NO_WINDOW,
          },
        );
      } catch (error) {
        this.markDead();
        return;
      }

      this.proc.on("error", () => this.markDead());
      this.proc.on("exit", () => this.markDead());
      if (this.proc.stdin) this.proc.stdin.on("error", () => this.markDead());

      if (this.proc.stdout) {
        this.proc.stdout.setEncoding("utf8");
        this.proc.stdout.on("data", (chunk) => this.onData(chunk));
      }

      // Add-Type compilation normally takes < 2 s; give it room but never
      // block a freeze forever if the worker fails to boot.
      setTimeout(() => this.finishStart(false), 15000).unref?.();
    });

    return this.startPromise;
  }

  onData(chunk) {
    this.buffer += chunk;
    let index;
    while ((index = this.buffer.indexOf("\n")) !== -1) {
      const line = this.buffer.slice(0, index).trim();
      this.buffer = this.buffer.slice(index + 1);
      this.onLine(line);
    }
  }

  onLine(line) {
    if (!line) return;
    if (line === "READY") {
      this.ready = true;
      this.finishStart(true);
      return;
    }
    if (line.startsWith("OK") || line.startsWith("ERR")) {
      const entry = this.pending.shift();
      if (!entry) return;
      clearTimeout(entry.timer);
      entry.resolve(line);
    }
    // Anything else (PowerShell noise) is ignored.
  }

  /** Send one command and wait for its reply. Rejects when not warm yet. */
  send(command) {
    if (!this.ready || this.dead || !this.proc || !this.proc.stdin) {
      return Promise.reject(new Error("chrome worker not ready"));
    }
    return new Promise((resolve, reject) => {
      const entry = { resolve, reject, timer: null };
      entry.timer = setTimeout(() => {
        const position = this.pending.indexOf(entry);
        if (position !== -1) this.pending.splice(position, 1);
        reject(new Error("chrome worker timeout"));
      }, 10000);
      entry.timer.unref?.();
      this.pending.push(entry);
      try {
        this.proc.stdin.write(`${command}\n`);
      } catch (error) {
        clearTimeout(entry.timer);
        const position = this.pending.indexOf(entry);
        if (position !== -1) this.pending.splice(position, 1);
        reject(error);
      }
    });
  }

  markDead() {
    this.dead = true;
    this.ready = false;
    this.finishStart(false);
    const pending = this.pending;
    this.pending = [];
    for (const entry of pending) {
      clearTimeout(entry.timer);
      entry.reject(new Error("chrome worker died"));
    }
  }

  stop() {
    if (!this.proc) return;
    try {
      if (this.ready && !this.dead && this.proc.stdin) {
        this.proc.stdin.write("EXIT\n");
      }
    } catch (error) {
      // ignore
    }
    const proc = this.proc;
    this.proc = null;
    this.markDead();
    setTimeout(() => {
      try {
        proc.kill();
      } catch (error) {
        // ignore
      }
    }, 300).unref?.();
  }
}

/* ------------------------------------------------------------------ *
 * One-shot fallback (slow, ~1.8 s) used only if the worker is unavailable.
 * ------------------------------------------------------------------ */

const ONE_SHOT_TYPES = `
if (-not ('KotobaProc' -as [type])) {
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class KotobaProc {
  [DllImport("ntdll.dll")] public static extern int NtSuspendProcess(IntPtr processHandle);
  [DllImport("ntdll.dll")] public static extern int NtResumeProcess(IntPtr processHandle);
}
"@
}
`;

const SUSPEND_SCRIPT = `${ONE_SHOT_TYPES}
$ErrorActionPreference = 'SilentlyContinue'
$pids = @()
Get-Process | Where-Object { $_.ProcessName -match '^(chrome|chromium|googlechrome)$' } | ForEach-Object {
  try {
    [KotobaProc]::NtSuspendProcess($_.Handle) | Out-Null
    $pids += $_.Id
  } catch {}
}
$pids | ForEach-Object { Write-Output $_ }
`;

function resumeScript(pids) {
  const ids = pids
    .map((pid) => Number(pid))
    .filter((pid) => Number.isInteger(pid) && pid > 0)
    .join(",");
  return `${ONE_SHOT_TYPES}
$ErrorActionPreference = 'SilentlyContinue'
@(${ids}) | ForEach-Object {
  $p = Get-Process -Id $_ -ErrorAction SilentlyContinue
  if ($p) { [KotobaProc]::NtResumeProcess($p.Handle) | Out-Null }
}
`;
}

function powershellArgs(script) {
  return [
    "-NoProfile",
    "-NonInteractive",
    "-WindowStyle",
    "Hidden",
    "-ExecutionPolicy",
    "Bypass",
    "-Command",
    script,
  ];
}

function runPowerShell(script) {
  return new Promise((resolve) => {
    execFile(
      powershellPath(),
      powershellArgs(script),
      { windowsHide: true, timeout: 30000, maxBuffer: 1024 * 1024 },
      (error, stdout) => {
        if (error) {
          resolve([]);
          return;
        }
        resolve(
          String(stdout || "")
            .split(/\r?\n/)
            .map((line) => Number(line.trim()))
            .filter((pid) => Number.isInteger(pid) && pid > 0),
        );
      },
    );
  });
}

function runPowerShellSync(script) {
  spawnSync(powershellPath(), powershellArgs(script), {
    windowsHide: true,
    timeout: 30000,
    maxBuffer: 1024 * 1024,
  });
}

/* ------------------------------------------------------------------ */

class ChromeBlocker {
  constructor() {
    this.suspended = new Set();
    this.shouldFreeze = false;
    this.chain = Promise.resolve();
    this.enabled = IS_WINDOWS;
    this.worker = IS_WINDOWS ? new PowerShellWorker() : null;
  }

  /**
   * Boot the PowerShell worker early (call once at app startup) so the first
   * freeze is instant. Idempotent.
   */
  warmup() {
    if (!this.worker) return Promise.resolve(false);
    return this.worker.start();
  }

  enqueue(work) {
    this.chain = this.chain.then(work).catch(() => {});
    return this.chain;
  }

  async suspendNow() {
    if (this.worker && !this.worker.dead) {
      try {
        if (!this.worker.ready) await this.worker.start();
        const reply = await this.worker.send("SUSPEND");
        const pids = String(reply)
          .slice(2)
          .split(",")
          .map((value) => Number(value.trim()))
          .filter((pid) => Number.isInteger(pid) && pid > 0);
        for (const pid of pids) this.suspended.add(pid);
        return;
      } catch (error) {
        // fall through to the one-shot path
      }
    }
    const pids = await runPowerShell(SUSPEND_SCRIPT);
    for (const pid of pids) this.suspended.add(pid);
  }

  async resumeNow(pids) {
    if (!pids.length) return;
    if (this.worker && !this.worker.dead) {
      try {
        if (!this.worker.ready) await this.worker.start();
        await this.worker.send(`RESUME ${pids.join(",")}`);
        return;
      } catch (error) {
        // fall through to the one-shot path
      }
    }
    await runPowerShell(resumeScript(pids));
  }

  sync() {
    return this.enqueue(async () => {
      if (this.shouldFreeze) {
        if (this.suspended.size === 0) {
          await this.suspendNow();
        }
      } else if (this.suspended.size > 0) {
        const pids = [...this.suspended];
        this.suspended.clear();
        await this.resumeNow(pids);
      }
    });
  }

  freeze() {
    if (!this.enabled) return;
    // A new freeze request after Chrome was resumed may happen while the first
    // suspend is still running; remember the latest requested state.
    this.shouldFreeze = true;
    this.sync();
  }

  unfreeze() {
    if (!this.enabled) return;
    this.shouldFreeze = false;
    this.sync();
  }

  shutdown() {
    if (!this.enabled) return;
    this.shouldFreeze = false;
    const pids = [...this.suspended];
    this.suspended.clear();
    if (pids.length) {
      // Synchronous resume: the app is quitting, the worker pipe may not flush
      // before the process exits, which would leave Chrome frozen forever.
      try {
        runPowerShellSync(resumeScript(pids));
      } catch (error) {
        // ignore
      }
    }
    if (this.worker) this.worker.stop();
  }
}

module.exports = { ChromeBlocker };
