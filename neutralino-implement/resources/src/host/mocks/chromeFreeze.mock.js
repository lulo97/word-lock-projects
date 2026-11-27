// Pure-JS stand-in for src/chromeFreeze/chromeFreeze.mock.js in the old app.
const calls = [];
let frozen = false;
let logPath = null;

function record(name) {
  calls.push(name);
  if (!logPath) return;
  // Fire and forget: logging is only used by tests.
  Neutralino.filesystem.readFile(logPath)
    .then((old) => Neutralino.filesystem.writeFile(logPath, `${old}${name}\n`))
    .catch(() => Neutralino.filesystem.writeFile(logPath, `${name}\n`).catch(() => {}));
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
  if (logPath) Neutralino.filesystem.writeFile(logPath, '').catch(() => {});
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

export const mock = { freeze, unfreeze, getCalls, isFrozen, reset, setLogPath, call };
