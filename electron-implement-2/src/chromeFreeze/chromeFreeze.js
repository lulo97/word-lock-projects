const { execFileSync } = require('node:child_process');

const PROCESS_SUSPEND_RESUME = 0x0800;
const DEFAULT_IMAGE_NAME = 'chrome.exe';

let apis = null;
const suspendedPids = new Set();

function loadApis() {
  if (apis !== null) return apis;

  if (process.platform !== 'win32') {
    apis = false;
    return apis;
  }

  try {
    const koffi = require('koffi');
    const kernel32 = koffi.load('kernel32.dll');
    const ntdll = koffi.load('ntdll.dll');

    apis = {
      OpenProcess: kernel32.func('void* OpenProcess(uint32 access, uint32 inherit, uint32 pid)'),
      CloseHandle: kernel32.func('uint32 CloseHandle(void* handle)'),
      NtSuspendProcess: ntdll.func('int32 NtSuspendProcess(void* handle)'),
      NtResumeProcess: ntdll.func('int32 NtResumeProcess(void* handle)'),
    };
  } catch {
    // Missing koffi / unsupported Windows build: fail open, never break the app.
    apis = false;
  }

  return apis;
}

function getPids(imageName = DEFAULT_IMAGE_NAME) {
  if (process.platform !== 'win32') return [];

  try {
    const output = execFileSync(
      'tasklist',
      ['/FI', `IMAGENAME eq ${imageName}`, '/FO', 'CSV', '/NH'],
      { encoding: 'utf8', windowsHide: true },
    );

    return output
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.startsWith('"'))
      .map((line) => {
        const columns = line.match(/"[^"]*"/g) ?? [];
        const pid = Number(columns[1]?.slice(1, -1));
        return Number.isInteger(pid) && pid > 0 ? pid : null;
      })
      .filter((pid) => pid !== null);
  } catch {
    return [];
  }
}

function applyToPids(pids, operation, onSuccess) {
  const api = loadApis();
  if (!api) return 0;

  let affected = 0;
  for (const pid of pids) {
    const handle = api.OpenProcess(PROCESS_SUSPEND_RESUME, 0, pid);
    if (!handle) continue;

    try {
      // NT_SUCCESS(status) is status >= 0.
      if (operation(api, handle) >= 0) {
        affected += 1;
        onSuccess(pid);
      }
    } catch {
      // Ignore one bad process and continue with the rest.
    } finally {
      try {
        api.CloseHandle(handle);
      } catch {
        // Nothing useful to do here.
      }
    }
  }

  return affected;
}

// Suspend is cumulative: only suspend processes we have not suspended already.
function freezeChrome(imageName = DEFAULT_IMAGE_NAME) {
  const pids = getPids(imageName).filter((pid) => !suspendedPids.has(pid));
  return applyToPids(pids, (api, handle) => api.NtSuspendProcess(handle), (pid) =>
    suspendedPids.add(pid),
  );
}

// Resume our own suspended PIDs plus any leftovers from a previous run.
// Resuming a process that is not suspended is harmless (best-effort cleanup).
function unfreezeChrome(imageName = DEFAULT_IMAGE_NAME) {
  const pids = [...new Set([...suspendedPids, ...getPids(imageName)])];
  const affected = applyToPids(
    pids,
    (api, handle) => api.NtResumeProcess(handle),
    (pid) => suspendedPids.delete(pid),
  );
  suspendedPids.clear();
  return affected;
}

module.exports = { freezeChrome, unfreezeChrome, getPids };
