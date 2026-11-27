// Pure-JS stand-in for src/chromeFreeze/chromeFreeze.mock.js in the old app.
const calls = [];
let frozen = false;
let logPath = null;
let logQueue = Promise.resolve();

function enqueue(action) {
  logQueue = logQueue.then(action).catch(() => {});
  return logQueue;
}

function record(name) {
  calls.push(name);
  if (!logPath) return;
  // Logging is only used by tests, but never drop a line and never let two
  // read-modify-write pairs race each other.
  enqueue(async () => {
    let old = '';
    try {
      old = await Neutralino.filesystem.readFile(logPath);
    } catch {
      old = '';
    }
    await Neutralino.filesystem.writeFile(logPath, `${old}${name}\n`).catch(() => {});
  });
}

export function flush() {
  return logQueue;
}

export function freeze() {
  record('freezeChrome');
  frozen = true;
  return { ok: true, count: 1 };
}

export function unfreeze() {
  record('unfreezeChrome');
  frozen = false;
  return { ok: true, count: 1 };
}

export function getCalls() {
  return [...calls];
}

export function isFrozen() {
  return frozen;
}

export function reset() {
  calls.length = 0;
  frozen = false;
  if (logPath) enqueue(() => Neutralino.filesystem.writeFile(logPath, '').catch(() => {}));
}

export function setLogPath(path) {
  logPath = path || null;
}

export function call(method) {
  switch (method) {
    case 'chrome.freeze': return freeze();
    case 'chrome.unfreeze': return unfreeze();
    case 'chrome.resumeAll': return unfreeze();
    case 'chrome.refreeze':
    default: return { ok: true };
  }
}

export const mock = { freeze, unfreeze, getCalls, isFrozen, reset, setLogPath, flush, call };
