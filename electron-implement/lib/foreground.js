/**
 * Bring the word gate to the Windows *keyboard* foreground.
 *
 * Electron's win.show() / win.focus() can only ask Windows to activate a
 * window. Windows blocks SetForegroundWindow() calls coming from a process that
 * did not receive the last input event, which is exactly the case for a tray app
 * that opens itself from a timer: the window becomes visible (always-on-top)
 * while the keyboard focus stays in whatever the user was typing in. The user
 * then has to click the answer field before typing.
 *
 * The reliable way around the foreground lock is the classic user32 dance:
 * attach to the current foreground thread's input queue, call
 * SetForegroundWindow() while attached, then detach. `SwitchToThisWindow()` is
 * used as a last resort because it is still allowed when the strict call is
 * denied. Everything runs inside one warm PowerShell process (Add-Type costs
 * ~1.5 s, so it is compiled once at startup, like lib/chrome.js does).
 */

const { spawn } = require("child_process");
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

const WORKER_SCRIPT = [
  "$ErrorActionPreference = 'SilentlyContinue'",
  "$cs = @'",
  "using System;",
  "using System.Runtime.InteropServices;",
  "public static class KotobaForeground {",
  '  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);',
  '  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();',
  '  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);',
  '  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);',
  '  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);',
  '  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);',
  '  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);',
  '  [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hWnd, bool fAltTab);',
  '  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);',
  '  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();',
  "}",
  "'@",
  "Add-Type -TypeDefinition $cs",
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
  "  if ($line -like 'FOCUS *') {",
  "    [long]$value = 0",
  "    $raw = $line.Substring(6).Trim()",
  "    if (-not ([long]::TryParse($raw, [ref]$value)) -or $value -eq 0) {",
  "      $out.WriteLine('ERR hwnd')",
  "      $out.Flush()",
  "      continue",
  "    }",
  "    $hwnd = [IntPtr]$value",
  "    if ([KotobaForeground]::IsIconic($hwnd)) {",
  "      [KotobaForeground]::ShowWindow($hwnd, 9) | Out-Null",
  "    }",
  "    $ok = ([KotobaForeground]::GetForegroundWindow() -eq $hwnd)",
  "    if (-not $ok) {",
  "      [uint32]$foregroundPid = 0",
  "      $foregroundThread = [KotobaForeground]::GetWindowThreadProcessId([KotobaForeground]::GetForegroundWindow(), [ref]$foregroundPid)",
  "      $currentThread = [KotobaForeground]::GetCurrentThreadId()",
  "      $attached = $false",
  "      if ($foregroundThread -ne 0 -and $foregroundThread -ne $currentThread) {",
  "        $attached = [KotobaForeground]::AttachThreadInput($currentThread, $foregroundThread, $true)",
  "      }",
  "      [KotobaForeground]::BringWindowToTop($hwnd) | Out-Null",
  "      $ok = [KotobaForeground]::SetForegroundWindow($hwnd)",
  "      if (-not $ok) {",
  "        # A synthetic ALT tap makes this process the 'last input' owner, which",
  "        # lifts the foreground lock for the retry.",
  "        [KotobaForeground]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)",
  "        [KotobaForeground]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)",
  "        $ok = [KotobaForeground]::SetForegroundWindow($hwnd)",
  "      }",
  "      if (-not $ok) {",
  "        [KotobaForeground]::SwitchToThisWindow($hwnd, $true)",
  "        $ok = ([KotobaForeground]::GetForegroundWindow() -eq $hwnd)",
  "      }",
  "      if ($attached) {",
  "        [KotobaForeground]::AttachThreadInput($currentThread, $foregroundThread, $false) | Out-Null",
  "      }",
  "    }",
  "    if ($ok) { $out.WriteLine('OK') } else { $out.WriteLine('ERR foreground') }",
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
  }

  send(command) {
    if (!this.ready || this.dead || !this.proc || !this.proc.stdin) {
      return Promise.reject(new Error("foreground worker not ready"));
    }
    return new Promise((resolve, reject) => {
      const entry = { resolve, reject, timer: null };
      entry.timer = setTimeout(() => {
        const position = this.pending.indexOf(entry);
        if (position !== -1) this.pending.splice(position, 1);
        reject(new Error("foreground worker timeout"));
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
      entry.reject(new Error("foreground worker died"));
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

/**
 * HWND of a BrowserWindow as a decimal number, or 0 when unavailable.
 * getNativeWindowHandle() returns the raw pointer (8 bytes on 64-bit).
 */
function windowHandle(win) {
  if (!win || win.isDestroyed()) return 0;
  try {
    const handle = win.getNativeWindowHandle();
    if (!handle || !handle.length) return 0;
    if (handle.length >= 8) return Number(handle.readBigUInt64LE(0));
    return handle.readUInt32LE(0);
  } catch (error) {
    return 0;
  }
}

class ForegroundBooster {
  constructor() {
    this.enabled = IS_WINDOWS;
    this.worker = IS_WINDOWS ? new PowerShellWorker() : null;
    this.chain = Promise.resolve();
  }

  /** Compile the helper once at startup so the first gate focuses instantly. */
  warmup() {
    if (!this.worker) return Promise.resolve(false);
    return this.worker.start();
  }

  /** Resolves true when the window is the Windows foreground window. */
  activate(hwnd) {
    if (!this.enabled || !this.worker) return Promise.resolve(false);
    const handle = Number(hwnd) || 0;
    if (!handle) return Promise.resolve(false);

    // Serialise requests: the helper answers one command at a time.
    const run = async () => {
      try {
        if (!this.worker.ready) {
          const started = await this.worker.start();
          if (!started) return false;
        }
        const reply = await this.worker.send(`FOCUS ${handle}`);
        return String(reply).startsWith("OK");
      } catch (error) {
        return false;
      }
    };
    this.chain = this.chain.then(run, run);
    return this.chain;
  }

  shutdown() {
    if (this.worker) this.worker.stop();
  }
}

module.exports = { ForegroundBooster, windowHandle };
